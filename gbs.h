/* $Id: gbs.h,v 1.4 2003/11/29 19:03:15 ranma Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 */

#ifndef _GBS_H_
#define _GBS_H_

#include <inttypes.h>

#define GBS_LEN_SHIFT	10
#define GBS_LEN_DIV	(1 << (GBS_LEN_SHIFT-1))

struct gbs_subsong_info {
	uint32_t len;
	char *title;
};

struct gbs {
	uint8_t *buf;
	uint32_t version;
	uint32_t songs;
	uint32_t defaultsong;
	uint32_t load;
	uint32_t init;
	uint32_t play;
	uint32_t stack;
	uint32_t tma;
	uint32_t tmc;
	char *title;
	char *author;
	char *copyright;
	uint32_t codelen;
	uint8_t *code;
	uint8_t *exthdr;
	uint32_t filesize;
	uint32_t crc;
	uint32_t crcnow;
	struct gbs_subsong_info *subsong_info;
	char *strings;
	char v1strings[33*3];
	uint8_t *rom;
	uint32_t romsize;
};

struct gbs *gbs_open(char *name);
int gbs_playsong(struct gbs *gbs, int i);
void gbs_printinfo(struct gbs *gbs, int verbose);
void gbs_close(struct gbs *gbs);
int gbs_write(struct gbs *gbs, char *name, int version);

#endif
