/* $Id: gbs.h,v 1.1 2003/08/27 15:07:03 ranma Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 */

#ifndef _GBS_H_
#define _GBS_H_

struct gbs_subsong_info {
	unsigned int len;
	unsigned char *title;
};

struct gbs {
	unsigned char *buf;
	unsigned int version;
	unsigned int songs;
	unsigned int defaultsong;
	unsigned int load;
	unsigned int init;
	unsigned int play;
	unsigned int stack;
	unsigned int tma;
	unsigned int tmc;
	char *title;
	char *author;
	char *copyright;
	unsigned int codelen;
	unsigned char *code;
	unsigned char *exthdr;
	unsigned int filesize;
	unsigned int crc;
	unsigned int crcnow;
	struct gbs_subsong_info *subsong_info;
	char *strings;
	char v1strings[33*3];
	unsigned char *rom;
	unsigned int romsize;
};

struct gbs *gbs_open(char *name);
int gbs_playsong(struct gbs *gbs, int i);
void gbs_printinfo(struct gbs *gbs, int verbose);
void gbs_close(struct gbs *gbs);
int gbs_write(struct gbs *gbs, char *name, int version);

#endif
