/* $Id: gbs.h,v 1.5 2003/11/30 14:35:59 ranma Exp $
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
	int codelen;
	uint8_t *code;
	uint8_t *exthdr;
	size_t filesize;
	uint32_t crc;
	uint32_t crcnow;
	struct gbs_subsong_info *subsong_info;
	char *strings;
	char v1strings[33*3];
	uint8_t *rom;
	int romsize;
};

struct gbs *gbs_open(char *name);
int gbs_playsong(struct gbs *gbs, int i);
void gbs_printinfo(struct gbs *gbs, int verbose);
void gbs_close(struct gbs *gbs);
int gbs_write(struct gbs *gbs, char *name, int version);

#endif
