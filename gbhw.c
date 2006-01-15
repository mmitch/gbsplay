/* $Id: gbhw.c,v 1.46 2006/01/15 00:01:29 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003-2006 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 * Licensed under GNU GPL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include "gbcpu.h"
#include "gbhw.h"

static uint8_t *rom;
static uint8_t intram[0x2000];
static uint8_t extram[0x2000];
static uint8_t ioregs[0x80];
static uint8_t hiram[0x80];
static long rombank = 1;
static long lastbank;

static const char dutylookup[4] = {
	1, 2, 4, 6
};

struct gbhw_channel gbhw_ch[4];

static long lminval, lmaxval, rminval, rmaxval;

#define MASTER_VOL_MIN	0
#define MASTER_VOL_MAX	(256*256)
static long master_volume;
static long master_fade;
static long master_dstvol;

static const long vblanktc = 70256; /* ~59.7 Hz (vblankctr)*/
static long vblankctr = 70256;
static long timertc = 70256;
static long timerctr = 70256;

static const long msec_cycles = GBHW_CLOCK/1000;

static long pause_output = 0;

static gbhw_callback_fn callback;
static void *callbackpriv;
static struct gbhw_buffer *soundbuf = NULL; /* externally visible output buffer */
static struct gbhw_buffer *impbuf = NULL;   /* internal impulse output buffer */

#define TAP1_15		0x4000;
#define TAP2_15		0x2000;
#define TAP1_7		0x0040;
#define TAP2_7		0x0020;

static uint32_t tap1 = TAP1_15;
static uint32_t tap2 = TAP2_15;
static uint32_t lfsr = 0xffffffff;

#define SOUND_DIV_MULT 0x10000LL

static long long sound_div_tc;
static const long main_div_tc = 32;
static long main_div;
static const long sweep_div_tc = 256;
static long sweep_div;

static long ch3pos;

#if 0
#define IMPULSE_RES 32
#define IMPULSE_WIDTH 11

/* Filtered sinc impulse at 32 subsample offsets, each totaling exactly 256 */
static const short base_impulse[IMPULSE_RES][IMPULSE_WIDTH] =
{
	{   4,  -5,   0,  -5, 134, 134,  -5,   0,  -5,   4,   0, },
	{   4,  -5,   0,  -6, 129, 138,  -3,   0,  -5,   4,   0, },
	{   4,  -5,   0,  -8, 124, 143,  -1,   0,  -5,   4,   0, },
	{   4,  -5,   0,  -9, 119, 149,   0,   0,  -5,   3,   0, },
	{   4,  -5,   0,  -9, 113, 154,   2,  -1,  -5,   3,   0, },
	{   4,  -5,   0, -10, 108, 157,   5,  -1,  -5,   3,   0, },
	{   4,  -4,   0, -11, 102, 161,   8,  -2,  -5,   3,   0, },
	{   3,  -4,   0, -11,  97, 165,  11,  -2,  -5,   2,   0, },
	{   3,  -4,   0, -11,  91, 169,  14,  -3,  -5,   2,   0, },
	{   3,  -3,   0, -11,  85, 169,  17,  -3,  -4,   2,   1, },
	{   3,  -3,   0, -11,  80, 172,  21,  -4,  -4,   1,   1, },
	{   3,  -2,   0, -11,  74, 174,  25,  -5,  -4,   1,   1, },
	{   3,  -2,   0, -10,  69, 174,  29,  -6,  -3,   1,   1, },
	{   2,  -2,  -1, -10,  63, 178,  34,  -6,  -3,   0,   1, },
	{   2,  -1,  -1,  -9,  58, 177,  38,  -7,  -2,   0,   1, },
	{   2,  -1,  -1,  -9,  53, 177,  43,  -8,  -2,   0,   2, },
	{   2,   0,  -2,  -8,  48, 176,  48,  -8,  -2,   0,   2, },
	{   2,   0,  -2,  -8,  43, 177,  53,  -9,  -1,  -1,   2, },
	{   1,   0,  -2,  -7,  38, 177,  58,  -9,  -1,  -1,   2, },
	{   1,   0,  -3,  -6,  34, 178,  63, -10,  -1,  -2,   2, },
	{   1,   1,  -3,  -6,  29, 174,  69, -10,   0,  -2,   3, },
	{   1,   1,  -4,  -5,  25, 174,  74, -11,   0,  -2,   3, },
	{   1,   1,  -4,  -4,  21, 172,  80, -11,   0,  -3,   3, },
	{   1,   2,  -4,  -3,  17, 169,  85, -11,   0,  -3,   3, },
	{   0,   2,  -5,  -3,  14, 169,  91, -11,   0,  -4,   3, },
	{   0,   2,  -5,  -2,  11, 165,  97, -11,   0,  -4,   3, },
	{   0,   3,  -5,  -2,   8, 161, 102, -11,   0,  -4,   4, },
	{   0,   3,  -5,  -1,   5, 157, 108, -10,   0,  -5,   4, },
	{   0,   3,  -5,  -1,   2, 154, 113,  -9,   0,  -5,   4, },
	{   0,   3,  -5,   0,   0, 149, 119,  -9,   0,  -5,   4, },
	{   0,   4,  -5,   0,  -1, 143, 124,  -8,   0,  -5,   4, },
	{   0,   4,  -5,   0,  -3, 138, 129,  -6,   0,  -5,   4, },
};
#else
#define IMPULSE_RES 128
#define IMPULSE_WIDTH 12

/* Filtered impulse at 128 subsample offsets, each totaling exactly 256 */
static const short base_impulse[IMPULSE_RES][IMPULSE_WIDTH] =
{
	{  1,  -3,   7, -13,  32, 212,  27, -11,   6,  -3,   1,   0,  }, /* sum: 256 */
	{  1,  -3,   7, -13,  31, 212,  29, -12,   6,  -3,   1,   0,  }, /* sum: 256 */
	{  1,  -3,   6, -12,  30, 211,  31, -13,   7,  -3,   1,   0,  }, /* sum: 256 */
	{  1,  -3,   6, -11,  27, 212,  32, -13,   7,  -3,   1,   0,  }, /* sum: 256 */
	{  1,  -3,   6, -11,  26, 212,  34, -14,   8,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -3,   6, -11,  25, 211,  36, -14,   8,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -3,   6, -10,  23, 211,  37, -14,   8,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -3,   6, -10,  22, 211,  39, -15,   8,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -3,   6, -10,  21, 211,  40, -15,   8,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -3,   5,  -8,  18, 211,  42, -16,   9,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -3,   5,  -8,  17, 211,  44, -17,   9,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -3,   5,  -8,  16, 210,  46, -17,   9,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -3,   5,  -7,  14, 210,  47, -17,   9,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -3,   5,  -7,  13, 210,  49, -18,   9,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -3,   5,  -7,  12, 209,  51, -18,   9,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -3,   5,  -6,  10, 209,  52, -18,   9,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -3,   4,  -5,   9, 208,  55, -19,   9,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -3,   4,  -5,   8, 208,  56, -19,   9,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -2,   3,  -4,   6, 207,  58, -20,  10,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -2,   3,  -4,   5, 207,  60, -21,  10,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -2,   3,  -4,   4, 206,  62, -21,  10,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -2,   3,  -4,   3, 206,  63, -21,  10,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -2,   3,  -3,   2, 204,  66, -22,  10,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -2,   3,  -3,   1, 204,  67, -22,  10,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -2,   2,  -2,   0, 203,  69, -22,  10,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -2,   2,  -1,  -2, 202,  71, -22,  10,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -2,   2,  -1,  -3, 202,  73, -23,  10,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -2,   2,  -1,  -4, 201,  75, -23,  10,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -2,   2,   0,  -5, 199,  77, -24,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  1,  -2,   2,   0,  -6, 199,  79, -25,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,  -1,   2,   0,  -7, 198,  81, -25,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,  -1,   1,   1,  -8, 197,  83, -25,  12,  -5,   1,   0,  }, /* sum: 256 */
	{  0,  -1,   1,   2,  -9, 196,  84, -25,  12,  -5,   1,   0,  }, /* sum: 256 */
	{  0,  -1,   1,   2, -10, 195,  87, -26,  12,  -5,   1,   0,  }, /* sum: 256 */
	{  0,  -1,   1,   2, -11, 194,  89, -26,  12,  -5,   1,   0,  }, /* sum: 256 */
	{  0,  -1,   1,   2, -11, 193,  90, -26,  12,  -5,   1,   0,  }, /* sum: 256 */
	{  0,   0,   0,   3, -13, 192,  92, -26,  12,  -5,   1,   0,  }, /* sum: 256 */
	{  0,   0,   0,   3, -13, 190,  94, -26,  12,  -5,   1,   0,  }, /* sum: 256 */
	{  0,   0,   0,   3, -14, 190,  96, -27,  12,  -5,   1,   0,  }, /* sum: 256 */
	{  0,   0,  -1,   4, -15, 189,  98, -27,  12,  -5,   1,   0,  }, /* sum: 256 */
	{  0,   0,  -1,   5, -16, 187, 100, -27,  12,  -5,   1,   0,  }, /* sum: 256 */
	{  0,   0,  -1,   5, -17, 186, 102, -27,  12,  -5,   1,   0,  }, /* sum: 256 */
	{  0,   0,  -1,   5, -17, 184, 104, -27,  12,  -5,   1,   0,  }, /* sum: 256 */
	{  0,   0,  -1,   5, -18, 184, 106, -28,  12,  -5,   1,   0,  }, /* sum: 256 */
	{  0,   0,  -1,   6, -19, 182, 108, -28,  12,  -5,   1,   0,  }, /* sum: 256 */
	{  0,   0,  -1,   6, -20, 181, 110, -28,  12,  -5,   1,   0,  }, /* sum: 256 */
	{  0,   0,  -1,   6, -20, 179, 112, -28,  12,  -5,   1,   0,  }, /* sum: 256 */
	{  0,   0,  -1,   6, -20, 177, 114, -28,  12,  -5,   1,   0,  }, /* sum: 256 */
	{  0,   0,  -1,   6, -21, 177, 115, -28,  12,  -5,   1,   0,  }, /* sum: 256 */
	{  0,   0,  -2,   7, -21, 175, 117, -28,  12,  -5,   1,   0,  }, /* sum: 256 */
	{  0,   0,  -2,   8, -23, 174, 119, -28,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   0,  -2,   8, -23, 172, 121, -28,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   0,  -2,   8, -23, 170, 124, -29,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   0,  -2,   8, -24, 169, 126, -29,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   0,  -2,   8, -24, 168, 127, -29,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   0,  -2,   8, -24, 166, 129, -29,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   0,  -2,   9, -25, 164, 131, -29,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -3,   9, -26, 163, 133, -29,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -3,   9, -26, 161, 135, -29,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -3,   9, -26, 159, 137, -29,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  10, -26, 157, 139, -29,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  10, -27, 157, 140, -29,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  10, -27, 155, 142, -29,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  10, -27, 153, 143, -28,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -28, 151, 145, -28,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -28, 149, 147, -28,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -28, 147, 149, -28,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -28, 145, 151, -28,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -28, 143, 153, -28,  11,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -29, 143, 154, -27,  10,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -29, 141, 156, -27,  10,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -29, 139, 157, -26,  10,  -4,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -29, 137, 159, -26,   9,  -3,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -29, 135, 161, -26,   9,  -3,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -29, 133, 163, -26,   9,  -3,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -29, 131, 164, -25,   9,  -3,   1,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -29, 130, 165, -25,   9,  -2,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -29, 128, 167, -24,   8,  -2,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -29, 126, 169, -24,   8,  -2,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -29, 124, 170, -23,   8,  -2,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -28, 121, 172, -23,   8,  -2,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -28, 119, 174, -23,   8,  -2,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -5,  12, -28, 117, 175, -22,   8,  -2,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -5,  12, -28, 116, 176, -21,   7,  -2,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -5,  12, -28, 114, 177, -20,   6,  -1,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -5,  12, -28, 112, 179, -20,   6,  -1,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -5,  12, -28, 110, 181, -20,   6,  -1,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -5,  12, -28, 108, 182, -19,   6,  -1,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -5,  12, -28, 107, 183, -18,   5,  -1,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -5,  12, -27, 104, 184, -17,   5,  -1,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -5,  12, -27, 102, 186, -17,   5,  -1,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -5,  12, -27, 100, 187, -16,   5,  -1,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -5,  12, -27,  99, 188, -15,   4,  -1,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -5,  12, -27,  97, 189, -14,   3,   0,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -5,  12, -26,  94, 191, -14,   3,   0,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -5,  12, -26,  92, 192, -13,   3,   0,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -5,  12, -26,  91, 192, -11,   2,   0,   0,   0,  }, /* sum: 256 */
	{  0,   1,  -5,  12, -26,  89, 194, -11,   2,   1,  -1,   0,  }, /* sum: 256 */
	{  0,   1,  -5,  12, -26,  87, 195, -10,   2,   1,  -1,   0,  }, /* sum: 256 */
	{  0,   1,  -5,  12, -25,  84, 196,  -9,   2,   1,  -1,   0,  }, /* sum: 256 */
	{  0,   1,  -5,  12, -25,  83, 197,  -8,   1,   1,  -1,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -25,  81, 198,  -7,   0,   2,  -1,   0,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -25,  79, 199,  -6,   0,   2,  -2,   1,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -24,  77, 199,  -5,   0,   2,  -2,   1,  }, /* sum: 256 */
	{  0,   1,  -4,  11, -24,  75, 201,  -4,  -1,   2,  -2,   1,  }, /* sum: 256 */
	{  0,   1,  -4,  10, -23,  74, 201,  -3,  -1,   2,  -2,   1,  }, /* sum: 256 */
	{  0,   1,  -4,  10, -23,  72, 202,  -2,  -1,   2,  -2,   1,  }, /* sum: 256 */
	{  0,   1,  -4,  10, -22,  69, 203,   0,  -2,   2,  -2,   1,  }, /* sum: 256 */
	{  0,   1,  -4,  10, -22,  68, 203,   1,  -2,   2,  -2,   1,  }, /* sum: 256 */
	{  0,   1,  -4,  10, -22,  66, 205,   1,  -3,   3,  -2,   1,  }, /* sum: 256 */
	{  0,   1,  -4,  10, -21,  64, 205,   2,  -3,   3,  -2,   1,  }, /* sum: 256 */
	{  0,   1,  -4,  10, -21,  62, 206,   4,  -4,   3,  -2,   1,  }, /* sum: 256 */
	{  0,   1,  -4,  10, -21,  61, 206,   5,  -4,   3,  -2,   1,  }, /* sum: 256 */
	{  0,   1,  -4,  10, -20,  58, 207,   6,  -4,   3,  -2,   1,  }, /* sum: 256 */
	{  0,   1,  -4,   9, -19,  57, 207,   8,  -5,   4,  -3,   1,  }, /* sum: 256 */
	{  0,   1,  -4,   9, -19,  55, 208,   9,  -5,   4,  -3,   1,  }, /* sum: 256 */
	{  0,   1,  -4,   9, -18,  53, 208,  10,  -6,   5,  -3,   1,  }, /* sum: 256 */
	{  0,   1,  -4,   9, -18,  51, 209,  12,  -7,   5,  -3,   1,  }, /* sum: 256 */
	{  0,   1,  -4,   9, -18,  50, 209,  13,  -7,   5,  -3,   1,  }, /* sum: 256 */
	{  0,   1,  -4,   9, -17,  47, 210,  14,  -7,   5,  -3,   1,  }, /* sum: 256 */
	{  0,   1,  -4,   9, -17,  46, 210,  16,  -8,   5,  -3,   1,  }, /* sum: 256 */
	{  0,   1,  -4,   9, -17,  44, 211,  17,  -8,   5,  -3,   1,  }, /* sum: 256 */
	{  0,   1,  -4,   9, -16,  42, 211,  18,  -8,   5,  -3,   1,  }, /* sum: 256 */
	{  0,   1,  -4,   8, -15,  41, 211,  20, -10,   6,  -3,   1,  }, /* sum: 256 */
	{  0,   1,  -4,   8, -15,  39, 212,  21, -10,   6,  -3,   1,  }, /* sum: 256 */
	{  0,   1,  -4,   8, -14,  37, 211,  23, -10,   6,  -3,   1,  }, /* sum: 256 */
	{  0,   1,  -4,   8, -14,  36, 211,  25, -11,   6,  -3,   1,  }, /* sum: 256 */
	{  0,   1,  -4,   8, -14,  34, 212,  26, -11,   6,  -3,   1,  }, /* sum: 256 */
};
#endif

static regparm uint32_t rom_get(uint32_t addr)
{
//	DPRINTF("rom_get(%04x)\n", addr);
	return rom[addr & 0x3fff];
}

static regparm uint32_t rombank_get(uint32_t addr)
{
//	DPRINTF("rombank_get(%04x)\n", addr);
	return rom[(addr & 0x3fff) + 0x4000*rombank];
}

static regparm uint32_t io_get(uint32_t addr)
{
	if (addr >= 0xff80 && addr <= 0xfffe) {
		return hiram[addr & 0x7f];
	}
	if (addr >= 0xff10 &&
	           addr <= 0xff3f) {
		return ioregs[addr & 0x7f];
	}
	if (addr == 0xff00) return 0;
	if (addr == 0xffff) return ioregs[0x7f];
	fprintf(stderr, "ioread from 0x%04x unimplemented.\n", (unsigned int)addr);
	DPRINTF("io_get(%04x)\n", addr);
	return 0xff;
}

static regparm uint32_t intram_get(uint32_t addr)
{
//	DPRINTF("intram_get(%04x)\n", addr);
	return intram[addr & 0x1fff];
}

static regparm uint32_t extram_get(uint32_t addr)
{
//	DPRINTF("extram_get(%04x)\n", addr);
	return extram[addr & 0x1fff];
}

static regparm void rom_put(uint32_t addr, uint8_t val)
{
	if (addr >= 0x2000 && addr <= 0x3fff) {
		val &= 0x1f;
		rombank = val + (val == 0);
		if (rombank > lastbank) {
			fprintf(stderr, "Bank %ld out of range (0-%ld)!\n", rombank, lastbank);
			rombank = lastbank;
		}
	}
}

static regparm void io_put(uint32_t addr, uint8_t val)
{
	long chn = (addr - 0xff10)/5;
	if (addr >= 0xff80 && addr <= 0xfffe) {
		hiram[addr & 0x7f] = val;
		return;
	}
	ioregs[addr & 0x7f] = val;
	DPRINTF(" ([0x%04x]=%02x) ", addr, val);
	switch (addr) {
		case 0xff06:
		case 0xff07:
			timertc = (256-ioregs[0x06]) * (16 << (((ioregs[0x07]+3) & 3) << 1));
			if ((ioregs[0x07] & 0xf0) == 0x80) timertc /= 2;
//			printf("Callback rate set to %2.2fHz.\n", GBHW_CLOCK/(float)timertc);
			break;
		case 0xff10:
			gbhw_ch[0].sweep_ctr = gbhw_ch[0].sweep_tc = ((val >> 4) & 7);
			gbhw_ch[0].sweep_dir = (val >> 3) & 1;
			gbhw_ch[0].sweep_shift = val & 7;

			break;
		case 0xff11:
		case 0xff16:
		case 0xff20:
			{
				long duty_ctr = val >> 6;
				long len = val & 0x3f;

				gbhw_ch[chn].duty_ctr = dutylookup[duty_ctr];
				gbhw_ch[chn].duty_tc = gbhw_ch[chn].div_tc*gbhw_ch[chn].duty_ctr/8;
				gbhw_ch[chn].len = (64 - len)*2;

				break;
			}
		case 0xff12:
		case 0xff17:
		case 0xff21:
			{
				long vol = val >> 4;
				long envdir = (val >> 3) & 1;
				long envspd = val & 7;

				gbhw_ch[chn].volume = vol;
				gbhw_ch[chn].env_dir = envdir;
				gbhw_ch[chn].env_ctr = gbhw_ch[chn].env_tc = envspd*8;
			}
			break;
		case 0xff13:
		case 0xff14:
		case 0xff18:
		case 0xff19:
		case 0xff1d:
		case 0xff1e:
			{
				unsigned long div = ioregs[0x13 + 5*chn];

				div |= ((long)ioregs[0x14 + 5*chn] & 7) << 8;
				gbhw_ch[chn].div_tc = 2048 - div;
				gbhw_ch[chn].duty_tc = gbhw_ch[chn].div_tc*gbhw_ch[chn].duty_ctr/8;

				if (addr == 0xff13 ||
				    addr == 0xff18 ||
				    addr == 0xff1d) break;
			}
			gbhw_ch[chn].len_enable = (ioregs[0x14 + 5*chn] & 0x40) > 0;

//			printf(" ch%ld: vol=%02d envd=%ld envspd=%ld duty_ctr=%ld len=%03d len_en=%ld key=%04d gate=%ld%ld\n", chn, gbhw_ch[chn].volume, gbhw_ch[chn].env_dir, gbhw_ch[chn].env_tc, gbhw_ch[chn].duty_ctr, gbhw_ch[chn].len, gbhw_ch[chn].len_enable, gbhw_ch[chn].div_tc, gbhw_ch[chn].leftgate, gbhw_ch[chn].rightgate);
			break;
		case 0xff15:
			break;
		case 0xff1a:
			gbhw_ch[2].master = (ioregs[0x1a] & 0x80) > 0;
			break;
		case 0xff1b:
			gbhw_ch[2].len = (256 - val)*2;
			break;
		case 0xff1c:
			{
				long vol = (ioregs[0x1c] >> 5) & 3;
				gbhw_ch[2].volume = vol;
				break;
			}
		case 0xff1f:
			break;
		case 0xff22:
		case 0xff23:
			{
				long div = ioregs[0x22];
				long shift = div >> 4;
				long rate = div & 7;
				gbhw_ch[3].div_ctr = 0;
				gbhw_ch[3].div_tc = 1 << shift;
				if (div & 8) {
					tap1 = TAP1_7;
					tap2 = TAP2_7;
				} else {
					tap1 = TAP1_15;
					tap2 = TAP2_15;
				}
				lfsr |= 1; /* Make sure lfsr is not 0 */
				if (rate) gbhw_ch[3].div_tc *= rate;
				else gbhw_ch[3].div_tc /= 2;
				if (addr == 0xff22) break;
//				printf(" ch4: vol=%02d envd=%ld envspd=%ld duty_ctr=%ld len=%03d len_en=%ld key=%04d gate=%ld%ld\n", gbhw_ch[3].volume, gbhw_ch[3].env_dir, gbhw_ch[3].env_ctr, gbhw_ch[3].duty_ctr, gbhw_ch[3].len, gbhw_ch[3].len_enable, gbhw_ch[3].div_tc, gbhw_ch[3].leftgate, gbhw_ch[3].rightgate);
			}
			gbhw_ch[chn].len_enable = (ioregs[0x23] & 0x40) > 0;
			break;
		case 0xff25:
			gbhw_ch[0].leftgate = (val & 0x10) > 0;
			gbhw_ch[0].rightgate = (val & 0x01) > 0;
			gbhw_ch[1].leftgate = (val & 0x20) > 0;
			gbhw_ch[1].rightgate = (val & 0x02) > 0;
			gbhw_ch[2].leftgate = (val & 0x40) > 0;
			gbhw_ch[2].rightgate = (val & 0x04) > 0;
			gbhw_ch[3].leftgate = (val & 0x80) > 0;
			gbhw_ch[3].rightgate = (val & 0x08) > 0;
			break;
		case 0xff26:
			ioregs[0x26] = 0x80;
			break;
		case 0xff00:
		case 0xff24:
		case 0xff27:
		case 0xff28:
		case 0xff29:
		case 0xff2a:
		case 0xff2b:
		case 0xff2c:
		case 0xff2d:
		case 0xff2e:
		case 0xff2f:
		case 0xff30:
		case 0xff31:
		case 0xff32:
		case 0xff33:
		case 0xff34:
		case 0xff35:
		case 0xff36:
		case 0xff37:
		case 0xff38:
		case 0xff39:
		case 0xff3a:
		case 0xff3b:
		case 0xff3c:
		case 0xff3d:
		case 0xff3e:
		case 0xff3f:
		case 0xffff:
			break;
		default:
			fprintf(stderr, "iowrite to 0x%04x unimplemented (val=%02x).\n", addr, val);
			break;
	}
}

static regparm void intram_put(uint32_t addr, uint8_t val)
{
	intram[addr & 0x1fff] = val;
}

static regparm void extram_put(uint32_t addr, uint8_t val)
{
	extram[addr & 0x1fff] = val;
}

static regparm void gb_sound_sweep(void)
{
	long i;

	if (gbhw_ch[0].sweep_tc) {
		gbhw_ch[0].sweep_ctr--;
		if (gbhw_ch[0].sweep_ctr < 0) {
			long val = gbhw_ch[0].div_tc >> gbhw_ch[0].sweep_shift;

			gbhw_ch[0].sweep_ctr = gbhw_ch[0].sweep_tc;
			if (gbhw_ch[0].sweep_dir) {
				if (gbhw_ch[0].div_tc < 2048 - val) gbhw_ch[0].div_tc += val;
			} else {
				if (gbhw_ch[0].div_tc > val) gbhw_ch[0].div_tc -= val;
			}
			gbhw_ch[0].duty_tc = gbhw_ch[0].div_tc*gbhw_ch[0].duty_ctr/8;
		}
	}
	for (i=0; i<4; i++) {
		if (gbhw_ch[i].len > 0 && gbhw_ch[i].len_enable) {
			gbhw_ch[i].len--;
			if (gbhw_ch[i].len == 0) {
				gbhw_ch[i].volume = 0;
				gbhw_ch[i].env_tc = 0;
			}
		}
		if (gbhw_ch[i].env_tc) {
			gbhw_ch[i].env_ctr--;
			if (gbhw_ch[i].env_ctr <=0) {
				gbhw_ch[i].env_ctr = gbhw_ch[i].env_tc;
				if (!gbhw_ch[i].env_dir) {
					if (gbhw_ch[i].volume > 0)
						gbhw_ch[i].volume--;
				} else {
					if (gbhw_ch[i].volume < 15)
					gbhw_ch[i].volume++;
				}
			}
		}
	}
	if (master_fade) {
		master_volume += master_fade;
		if ((master_fade > 0 &&
		     master_volume >= master_dstvol) ||
		    (master_fade < 0 &&
		     master_volume <= master_dstvol)) {
			master_fade = 0;
			master_volume = master_dstvol;
		}
	}

}

regparm void gbhw_master_fade(long speed, long dstvol)
{
	if (dstvol < MASTER_VOL_MIN) dstvol = MASTER_VOL_MIN;
	if (dstvol > MASTER_VOL_MAX) dstvol = MASTER_VOL_MAX;
	master_dstvol = dstvol;
	if (dstvol > master_volume)
		master_fade = speed;
	else master_fade = -speed;
}

#define GET_NIBBLE(p, n) ({ \
	long index = ((n) >> 1) & 0xf; \
	long shift = (~(n) & 1) << 2; \
	(((p)[index] >> shift) & 0xf); })

static regparm void gb_flush_buffer(void)
{
	long i;
	long overlap;
	long l_smpl, r_smpl;

	/* integrate buffer */
	l_smpl = soundbuf->l_lvl;
	r_smpl = soundbuf->r_lvl;
	for (i=0; i<soundbuf->samples; i++) {
		l_smpl = soundbuf->data[i*2  ] = l_smpl + impbuf->data[i*2  ];
		r_smpl = soundbuf->data[i*2+1] = r_smpl + impbuf->data[i*2+1];
		if (l_smpl > lmaxval) lmaxval = l_smpl;
		if (l_smpl < lminval) lminval = l_smpl;
		if (r_smpl > rmaxval) rmaxval = r_smpl;
		if (r_smpl < rminval) rminval = r_smpl;
	}
	soundbuf->pos = soundbuf->samples;
	soundbuf->l_lvl = l_smpl;
	soundbuf->r_lvl = r_smpl;

	if (callback != NULL) callback(soundbuf, callbackpriv);

	overlap = impbuf->samples - soundbuf->samples;
	memmove(impbuf->data, impbuf->data+(2*soundbuf->samples), 4*overlap);
	memset(impbuf->data + 2*overlap, 0, impbuf->bytes - 4*overlap);
	assert(impbuf->bytes == impbuf->samples*4);
	assert(soundbuf->bytes == soundbuf->samples*4);
	memset(soundbuf->data, 0, soundbuf->bytes);
	soundbuf->pos = 0;

	impbuf->cycles -= (sound_div_tc * soundbuf->samples) / SOUND_DIV_MULT;
}

static regparm void gb_change_level(long l_ofs, long r_ofs)
{
	long pos = impbuf->cycles * SOUND_DIV_MULT / sound_div_tc;
	long imp_idx = (impbuf->cycles*IMPULSE_RES*SOUND_DIV_MULT / sound_div_tc) % IMPULSE_RES;
	long imp_l = -IMPULSE_WIDTH/2;
	long imp_r = IMPULSE_WIDTH/2;
	long i;

	assert(pos + imp_r < impbuf->samples);
	assert(pos + imp_l >= 0);

	for (i=imp_l; i<imp_r; i++) {
		long bufi = pos + i;
		long impi = i + IMPULSE_WIDTH/2;
		impbuf->data[bufi*2  ] += base_impulse[imp_idx][impi] * l_ofs;
		impbuf->data[bufi*2+1] += base_impulse[imp_idx][impi] * r_ofs;
	}

	impbuf->l_lvl += l_ofs*256;
	impbuf->r_lvl += r_ofs*256;
}

static regparm void gb_sound(long cycles)
{
	long i;
	int l_lvl = 0, r_lvl = 0;
	static int old_l = 0, old_r = 0;

	if (gbhw_ch[2].master) for (i=0; i<cycles; i++) {
		gbhw_ch[2].div_ctr--;
		if (gbhw_ch[2].div_ctr <= 0) {
			gbhw_ch[2].div_ctr = gbhw_ch[2].div_tc*2;
			ch3pos++;
		}
	}
	main_div += cycles;
	while (main_div > main_div_tc) {
		main_div -= main_div_tc;
		impbuf->cycles += main_div_tc;
		if (impbuf->cycles*SOUND_DIV_MULT >= sound_div_tc*(impbuf->samples - IMPULSE_WIDTH/2))
			gb_flush_buffer();

		for (i=0; i<2; i++) if (gbhw_ch[i].master) {
			long val = gbhw_ch[i].volume;
			if (gbhw_ch[i].div_ctr > gbhw_ch[i].duty_tc) {
				val = -val;
			}
			if (!gbhw_ch[i].mute) {
				if (gbhw_ch[i].leftgate)
					gbhw_ch[i].l_lvl = val;
				if (gbhw_ch[i].rightgate)
					gbhw_ch[i].r_lvl = val;
			}
			gbhw_ch[i].div_ctr--;
			if (gbhw_ch[i].div_ctr <= 0) {
				gbhw_ch[i].div_ctr = gbhw_ch[i].div_tc;
			}
		}
		for (i=0; i<2; i++) {
			l_lvl += gbhw_ch[i].l_lvl;
			r_lvl += gbhw_ch[i].r_lvl;
		}
		if (gbhw_ch[2].master) {
			long pos = ch3pos;
			long val = GET_NIBBLE(&ioregs[0x30], pos);
			if (gbhw_ch[2].volume) {
				val = val >> (gbhw_ch[2].volume-1);
			} else val = 0;
			val = val*2;
			if (gbhw_ch[2].volume && !gbhw_ch[2].mute) {
				if (gbhw_ch[2].leftgate)
					gbhw_ch[2].l_lvl = val;
				if (gbhw_ch[2].rightgate)
					gbhw_ch[2].r_lvl = val;
			}
		}
		l_lvl += gbhw_ch[2].l_lvl;
		r_lvl += gbhw_ch[2].r_lvl;

		if (gbhw_ch[3].master) {
//			long val = gbhw_ch[3].volume * (((lfsr >> 13) & 2)-1);
//			long val = gbhw_ch[3].volume * ((random() & 2)-1);
			static long val;
			if (!gbhw_ch[3].mute) {
				if (gbhw_ch[3].leftgate)
					gbhw_ch[3].l_lvl = val;
				if (gbhw_ch[3].rightgate)
					gbhw_ch[3].r_lvl = val;
			}
			gbhw_ch[3].div_ctr--;
			if (gbhw_ch[3].div_ctr <= 0) {
				gbhw_ch[3].div_ctr = gbhw_ch[3].div_tc;
				lfsr = (lfsr << 1) | (((lfsr & tap1) > 0) ^ ((lfsr & tap2) > 0));
//				val = gbhw_ch[3].volume * ((random() & 2)-1);
				val = gbhw_ch[3].volume * ((lfsr & 2)-1);
			}
		}
		l_lvl += gbhw_ch[3].l_lvl;
		r_lvl += gbhw_ch[3].r_lvl;

		l_lvl = (l_lvl * master_volume) / MASTER_VOL_MAX;
		r_lvl = (r_lvl * master_volume) / MASTER_VOL_MAX;
		if (l_lvl != old_l || r_lvl != old_r) {
			gb_change_level(l_lvl - old_l, r_lvl - old_r);
			old_l = l_lvl;
			old_r = r_lvl;
		}

		sweep_div += 1;
		if (sweep_div >= sweep_div_tc) {
			sweep_div = 0;
			gb_sound_sweep();
		}
	}
}

regparm void gbhw_setcallback(gbhw_callback_fn fn, void *priv)
{
	callback = fn;
	callbackpriv = priv;
}

regparm void gbhw_setbuffer(struct gbhw_buffer *buffer)
{
	soundbuf = buffer;
	soundbuf->samples = soundbuf->bytes / 4;

	if (impbuf) free(impbuf);
	impbuf = malloc(sizeof(*impbuf) + (soundbuf->samples + IMPULSE_WIDTH + 1) * 4);
	memset(impbuf, 0, sizeof(*impbuf));
	impbuf->data = (void*)(impbuf+1);
	impbuf->samples = soundbuf->samples + IMPULSE_WIDTH + 1;
	impbuf->bytes = impbuf->samples * 4;
}

regparm void gbhw_setrate(long rate)
{
	sound_div_tc = GBHW_CLOCK*SOUND_DIV_MULT/rate;
}

regparm void gbhw_getminmax(int16_t *lmin, int16_t *lmax, int16_t *rmin, int16_t *rmax)
{
	if (lminval == INT_MAX) return;
	*lmin = lminval;
	*lmax = lmaxval;
	*rmin = rminval;
	*rmax = rmaxval;
	lminval = rminval = INT_MAX;
	lmaxval = rmaxval = INT_MIN;
}

/*
 * Initialize Gameboy hardware emulation.
 * The size should be a multiple of 0x4000,
 * so we don't need range checking in rom_get and
 * rombank_get.
 */
regparm void gbhw_init(uint8_t *rombuf, uint32_t size)
{
	long i;

	rom = rombuf;
	lastbank = ((size + 0x3fff) / 0x4000) - 1;
	rombank = 1;
	memset(gbhw_ch, 0, sizeof(gbhw_ch));
	master_volume = MASTER_VOL_MAX;
	master_fade = 0;
	if (soundbuf)
		soundbuf->pos = 0;
	lminval = rminval = INT_MAX;
	lmaxval = rmaxval = INT_MIN;
	for (i=0; i<4; i++) {
		gbhw_ch[i].duty_ctr = 4;
		gbhw_ch[i].div_tc = 1;
		gbhw_ch[i].master = 1;
		gbhw_ch[i].mute = 0;
	}
	memset(extram, 0, sizeof(extram));
	memset(intram, 0, sizeof(intram));
	memset(hiram, 0, sizeof(hiram));
	memset(ioregs, 0, sizeof(ioregs));
	gbcpu_init();
	gbcpu_addmem(0x00, 0x3f, rom_put, rom_get);
	gbcpu_addmem(0x40, 0x7f, rom_put, rombank_get);
	gbcpu_addmem(0xa0, 0xbf, extram_put, extram_get);
	gbcpu_addmem(0xc0, 0xfe, intram_put, intram_get);
	gbcpu_addmem(0xff, 0xff, io_put, io_get);
}

/**
 * @param time_to_work  emulated time in milliseconds
 * @return  elapsed cpu cycles
 */
regparm long gbhw_step(long time_to_work)
{
	long cycles_total = 0;

	if (pause_output) {
		usleep(time_to_work*1000);
		return 0;
	}

	time_to_work *= msec_cycles;
	
	while (cycles_total < time_to_work) {
		long maxcycles = time_to_work - cycles_total;
		long cycles = 0;

		if (vblankctr > 0 && vblankctr < maxcycles) maxcycles = vblankctr;
		if (timerctr > 0 && timerctr < maxcycles) maxcycles = timerctr;

		while (cycles < maxcycles) {
			long step = gbcpu_step();
			if (step < 0) return step;
			cycles += step;
			gb_sound(step);
		}

		if (vblankctr > 0) vblankctr -= cycles;
		if (vblankctr <= 0 && gbcpu_if && (ioregs[0x7f] & 1)) {
			vblankctr += vblanktc;
			gbcpu_intr(0x40);
		}
		if (timerctr > 0) timerctr -= cycles;
		if (timerctr <= 0 && gbcpu_if && (ioregs[0x7f] & 4)) {
			timerctr += timertc;
			gbcpu_intr(0x48);
		}
		cycles_total += cycles;
	}

	return cycles_total;
}

regparm void gbhw_pause(long new_pause)
{
	pause_output = new_pause != 0;
}
