/*
 * gbsplay is a Gameboy sound player
 * This file contains the player code common to both CLI and X11 frontends.
 *
 * 2003-2005,2008,2018,2020 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                                 Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#ifndef _PLAYER_H_
#define _PLAYER_H_

#include "common.h"

#include "gbs.h"
#include "plugout.h"

#define LN2 .69314718055994530941
#define MAGIC 5.78135971352465960412
#define FREQ(x) (262144 / x)
/* #define NOTE(x) ((log(FREQ(x))/LN2 - log(55)/LN2)*12 + .2) */
#define NOTE(x) ((long)((log(FREQ(x))/LN2 - MAGIC)*12 + .2))

#define MAXOCTAVE 9

/* player modes */
#define PLAYMODE_LINEAR  1
#define PLAYMODE_RANDOM  2
#define PLAYMODE_SHUFFLE 3

/* global variables */
extern char *myname;
extern long quit;
extern long pause_mode;

extern unsigned long random_seed;

extern long refresh_delay;

/* default values */
extern long playmode;
extern long loopmode;
extern enum plugout_endian endian;
extern long rate;
extern long silence_timeout;
extern long fadeout;
extern long subsong_gap;
extern long subsong_start;
extern long subsong_stop;
extern long subsong_timeout;

extern const char cfgfile[];

extern char *sound_name;
extern char *filter_type;
extern char *sound_description;
extern plugout_open_fn  sound_open;
extern plugout_skip_fn  sound_skip;
extern plugout_pause_fn sound_pause;
extern plugout_io_fn    sound_io;
extern plugout_write_fn sound_write;
extern plugout_close_fn sound_close;

extern struct gbhw_buffer buf;

regparm void swap_endian(struct gbhw_buffer *buf);
regparm void iocallback(long cycles, uint32_t addr, uint8_t val, void *priv);
regparm void callback(struct gbhw_buffer *buf, void *priv);
regparm long *setup_playlist(long songs);
regparm long get_next_subsong(struct gbs *gbs);
regparm int get_prev_subsong(struct gbs *gbs);
regparm void setup_playmode(struct gbs *gbs);
regparm long nextsubsong_cb(struct gbs *gbs, void *priv);
char *endian_str(long endian);
regparm void version(void);

#endif
