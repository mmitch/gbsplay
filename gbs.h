/* $Id: gbs.h,v 1.9 2004/03/10 01:48:15 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 */

#ifndef _GBS_H_
#define _GBS_H_

#include <inttypes.h>
#include "common.h"

#define GBS_LEN_SHIFT	10
#define GBS_LEN_DIV	(1 << GBS_LEN_SHIFT)

struct gbs;

typedef regparm int (*gbs_nextsubsong_cb)(struct gbs *gbs, void *priv);

struct gbs_subsong_info {
	uint32_t len;
	char *title;
};

struct gbs {
	uint8_t *buf;
	int version;
	int songs;
	int defaultsong;
	uint16_t load;
	uint16_t init;
	uint16_t play;
	uint16_t stack;
	uint8_t tma;
	uint8_t tmc;
	char *title;
	char *author;
	char *copyright;
	unsigned int codelen;
	uint8_t *code;
	uint8_t *exthdr;
	size_t filesize;
	uint32_t crc;
	uint32_t crcnow;
	struct gbs_subsong_info *subsong_info;
	char *strings;
	char v1strings[33*3];
	uint8_t *rom;
	unsigned int romsize;

	long long ticks;
	int16_t lmin, lmax, lvol, rmin, rmax, rvol;
	int subsong_timeout, silence_timeout, fadeout, gap;
	long long silence_start;
	int subsong;
	gbs_nextsubsong_cb nextsubsong_cb;
	void *nextsubsong_cb_priv;
};

regparm struct gbs *gbs_open(char *name);
regparm int gbs_init(struct gbs *gbs, int subsong);
regparm int gbs_step(struct gbs *gbs, int time_to_work);
regparm void gbs_set_nextsubsong_cb(struct gbs *gbs, gbs_nextsubsong_cb cb, void *priv);
regparm void gbs_printinfo(struct gbs *gbs, int verbose);
regparm void gbs_close(struct gbs *gbs);
regparm int gbs_write(struct gbs *gbs, char *name, int version);

#endif
