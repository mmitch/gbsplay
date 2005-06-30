/* $Id: gbs.h,v 1.11 2005/06/30 00:55:57 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003-2005 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 * Licensed under GNU GPL.
 */

#ifndef _GBS_H_
#define _GBS_H_

#include <inttypes.h>
#include "common.h"

#define GBS_LEN_SHIFT	10
#define GBS_LEN_DIV	(1 << GBS_LEN_SHIFT)

struct gbs;

typedef regparm long (*gbs_nextsubsong_cb)(struct gbs *gbs, void *priv);

struct gbs_subsong_info {
	uint32_t len;
	char *title;
};

struct gbs {
	uint8_t *buf;
	long version;
	long songs;
	long defaultsong;
	uint16_t load;
	uint16_t init;
	uint16_t play;
	uint16_t stack;
	uint8_t tma;
	uint8_t tmc;
	char *title;
	char *author;
	char *copyright;
	unsigned long codelen;
	uint8_t *code;
	uint8_t *exthdr;
	size_t filesize;
	uint32_t crc;
	uint32_t crcnow;
	struct gbs_subsong_info *subsong_info;
	char *strings;
	char v1strings[33*3];
	uint8_t *rom;
	unsigned long romsize;

	long long ticks;
	int16_t lmin, lmax, lvol, rmin, rmax, rvol;
	long subsong_timeout, silence_timeout, fadeout, gap;
	long long silence_start;
	long subsong;
	gbs_nextsubsong_cb nextsubsong_cb;
	void *nextsubsong_cb_priv;
};

regparm struct gbs *gbs_open(char *name);
regparm long gbs_init(struct gbs *gbs, long subsong);
regparm long gbs_step(struct gbs *gbs, long time_to_work);
regparm void gbs_set_nextsubsong_cb(struct gbs *gbs, gbs_nextsubsong_cb cb, void *priv);
regparm void gbs_printinfo(struct gbs *gbs, long verbose);
regparm void gbs_close(struct gbs *gbs);
regparm long gbs_write(struct gbs *gbs, char *name, long version);

#endif
