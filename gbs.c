/* $Id: gbs.c,v 1.6 2003/10/21 22:34:29 ranma Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Tobias Diedrich <ranma@gmx.at>
 *             Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "gbhw.h"
#include "gbcpu.h"
#include "gbs.h"
#include "crc32.h"

static const char playercode[] = {
	0xf5,              /* 0050:  push af         */
	0xe5,              /* 0051:  push hl         */
	0x01, 0x30, 0x00,  /* 0052:  ld   bc, 0x0030 */
	0x11, 0x10, 0xff,  /* 0055:  ld   de, 0xff10 */
	0x21, 0x9f, 0x00,  /* 0058:  ld   hl, 0x009f */
	0x2a,              /* 005b:  ldi  a, [hl]    */
	0x12,              /* 005c:  ld   [de], a    */
	0x13,              /* 005d:  inc  de         */
	0x0b,              /* 005e:  dec  bc         */
	0x78,              /* 005f:  ld   a, b       */
	0xb1,              /* 0060:  or   a, c       */
	0x20, 0xf8,        /* 0061:  jr nz $-0x08    */
	0xe1,              /* 0063:  pop  hl         */
	0xe5,              /* 0064:  push hl         */
	0x01, 0x0e, 0x00,  /* 0065:  ld   bc, 0x000e */
	0x09,              /* 0068:  add  hl, bc     */
	0x2a,              /* 0069:  ldi  a, [hl]    */
	0xe0, 0x06,        /* 006a:  ldh  [0x06], a  */
	0x2a,              /* 006c:  ldi  a, [hl]    */
	0xe0, 0x07,        /* 006d:  ldh  [0x07], a  */
	0x11, 0xff, 0xff,  /* 006f:  ld   de, 0xffff */
	0xcb, 0x57,        /* 0072:  bit  2, a       */
	0x3e, 0x01,        /* 0074:  ld   a, 0x01    */
	0x28, 0x02,        /* 0076:  jr z $+2        */
	0x3e, 0x04,        /* 0078:  ld   a, 0x04    */
	0x12,              /* 007a:  ld   [de], a    */
	0xe1,              /* 007b:  pop  hl         */
	0xf1,              /* 007c:  pop  af         */
	0x57,              /* 007d:  ld   d, a       */
	0xe5,              /* 007e:  push hl         */
	0x01, 0x08, 0x00,  /* 007f:  ld   bc, 0x0008 */
	0x09,              /* 0082:  add  hl, bc     */
	0x2a,              /* 0083:  ldi  a, [hl]    */
	0x66,              /* 0084:  ld   h, [hl]    */
	0x6f,              /* 0085:  ld   l, a       */
	0x7a,              /* 0086:  ld   a, d       */
	0x01, 0x8c, 0x00,  /* 0087:  ld   bc, 0x008c */
	0xc5,              /* 008a:  push bc         */
	0xe9,              /* 008b:  jp   hl         */
	0xfb,              /* 008c:  ei              */
	0x76,              /* 008d:  halt            */
	0xe1,              /* 008e:  pop  hl         */
	0xe5,              /* 008f:  push hl         */
	0x01, 0x0a, 0x00,  /* 0090:  ld   bc, 0x000a */
	0x09,              /* 0093:  add  hl, bc     */
	0x2a,              /* 0094:  ldi  a, [hl]    */
	0x66,              /* 0095:  ld   h, [hl]    */
	0x6f,              /* 0096:  ld   l, a       */
	0x7a,              /* 0097:  ld   a, d       */
	0x01, 0x9d, 0x00,  /* 0098:  ld   bc, 0x009d */
	0xc5,              /* 009b:  push bc         */
	0xe9,              /* 009c:  jp   hl         */
	0x18, 0xee,        /* 009d:  jr $-6          */

	0x80, 0xbf, 0x00, 0x00, 0xbf, /* 009f: initdata */
	0x00, 0x3f, 0x00, 0x00, 0xbf,
	0x7f, 0xff, 0x9f, 0x00, 0xbf,
	0x00, 0xff, 0x00, 0x00, 0xbf,
	0x77, 0xf3, 0xf1, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0xac, 0xdd, 0xda, 0x48,
	0x36, 0x02, 0xcf, 0x16,
	0x2c, 0x04, 0xe5, 0x2c,
	0xac, 0xdd, 0xda, 0x48
};

int gbs_playsong(struct gbs *gbs, int i)
{
	gbhw_init(gbs->rom, gbs->romsize);

	if (i == -1) i = gbs->defaultsong - 1;
	if (i >= gbs->songs) {
		fprintf(stderr, "Subsong number out of range (min=0, max=%d).\n", gbs->songs - 1);
		return 0;
	}

	gbcpu_if = 0;
	gbcpu_halted = 0;
	REGS16_W(gbcpu_regs, PC, 0x0050); /* playercode entry point */
	REGS16_W(gbcpu_regs, SP, gbs->stack);
	REGS16_W(gbcpu_regs, HL, gbs->load - 0x70);
	gbcpu_regs.rn.a = i;

	return 1;
}

void gbs_printinfo(struct gbs *gbs, int verbose)
{
	printf("GBSVersion:	%d.\n"
	       "Title:		\"%s\".\n"
	       "Author:		\"%s\".\n"
	       "Copyright:	\"%s\".\n"
	       "Load address:	0x%04x.\n"
	       "Init address:	0x%04x.\n"
	       "Play address:	0x%04x.\n"
	       "Stack pointer:	0x%04x.\n"
	       "File size:	0x%08x.\n"
	       "ROM size:	0x%08x (%d banks).\n"
	       "Subsongs:	%d.\n",
	       gbs->version,
	       gbs->title,
	       gbs->author,
	       gbs->copyright,
	       gbs->load,
	       gbs->init,
	       gbs->play,
	       gbs->stack,
	       gbs->filesize,
	       gbs->romsize,
	       gbs->romsize/0x4000,
	       gbs->songs);
	if (gbs->version == 2) {
		printf("CRC32:		0x%08x/0x%08x (%s).\n",
		       gbs->crc, gbs->crcnow,
		       gbs->crc == gbs->crcnow ? "OK" : "Failed");
	}
	if (verbose && gbs->version == 2) {
		int i;
		for (i=0; i<gbs->songs; i++) {
			printf("Subsong %03d:	", i);
			if (gbs->subsong_info[i].title) {
				printf("\"%s\" ", gbs->subsong_info[i].title);
			} else {
				printf("untitled ");
			}
			if (gbs->subsong_info[i].len) {
				printf("(%d seconds).\n",
				       gbs->subsong_info[i].len);
			} else {
				printf("(no timelimit).\n");
			}
		}
	}
}

void gbs_close(struct gbs *gbs)
{
	free(gbs->subsong_info);
	free(gbs);
}

void writeint(unsigned char *buf, unsigned int val, int bytes)
{
	int shift = 0;
	int i;

	for (i=0; i<bytes; i++) {
		buf[i] = (val >> shift) & 0xff;
		shift += 8;
	}
}

int gbs_write(struct gbs *gbs, char *name, int version)
{
	int fd;
	unsigned int codelen = (gbs->codelen + 15) >> 4;
	char pad[16];
	char strings[65536];
	int stringofs = 0;

	memset(pad, 0xff, sizeof(pad));

	if ((fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1) {
		fprintf(stderr, "Could not open %s: %s\n", name, strerror(errno));
		return 0;
	}

	if (version == 2) {
		int len,i;
		int newlen = 0x70 + codelen*16 + 16 + 4*gbs->songs;

		gbs->buf = realloc(gbs->buf, newlen + 65536);
		gbs->code = gbs->buf + 0x70;
		writeint(gbs->buf + 0x6e, codelen, 2);
		gbs->filesize = 0x70 + codelen*16 + 16 + 4*gbs->songs;
		memset(&gbs->code[gbs->codelen], 0xff, codelen*16 - gbs->codelen);
		memset(&gbs->code[16*codelen], 0x00, newlen - 0x70 - 16*codelen);
		gbs->exthdr = gbs->code + codelen*16;

		if ((len = strlen(gbs->title)) > 32) {
			memcpy(strings+stringofs, gbs->title, len+1);
			writeint(&gbs->exthdr[0x08], stringofs, 2);
			stringofs += len+1;
		} else writeint(&gbs->exthdr[0x08], 0xffff, 2);
		if ((len = strlen(gbs->author)) > 32) {
			memcpy(strings+stringofs, gbs->author, len+1);
			writeint(&gbs->exthdr[0x0a], stringofs, 2);
			stringofs += len+1;
		} else writeint(&gbs->exthdr[0x0a], 0xffff, 2);
		if ((len = strlen(gbs->copyright)) > 30) {
			memcpy(strings+stringofs, gbs->copyright, len+1);
			writeint(&gbs->exthdr[0x0c], stringofs, 2);
			stringofs += len+1;
		} else writeint(&gbs->exthdr[0x0c], 0xffff, 2);
			writeint(&gbs->exthdr[0x0e], 0, 2);

		for (i=0; i<gbs->songs; i++) {
			writeint(&gbs->exthdr[0x10+4*i],
			         gbs->subsong_info[i].len, 2);
			if (gbs->subsong_info[i].title &&
			    strcmp(gbs->subsong_info[i].title, "") != 0) {
				len = strlen(gbs->subsong_info[i].title)+1;
				memcpy(strings+stringofs, gbs->subsong_info[i].title, len);
				writeint(&gbs->exthdr[0x10+4*i+2],
				         stringofs, 2);
				stringofs += len;
			} else writeint(&gbs->exthdr[0x10+4*i+2], 0xffff, 2);
		}
		memcpy(gbs->buf + gbs->filesize, strings, stringofs);
		gbs->filesize += stringofs;

		writeint(&gbs->exthdr[0x00], gbs->filesize, 4);
		gbs->buf[3] = 2;

		gbs->crc = gbs_crc32(0, gbs->buf, gbs->filesize);
		writeint(&gbs->exthdr[0x04], gbs->crc, 4);
	} else {
		if (gbs->version == 2) {
			gbs->buf[3] = 1;
			gbs->filesize = 0x70 + gbs->codelen;
		}
	}
	write(fd, gbs->buf, gbs->filesize);
	close(fd);

	return 1;
}

struct gbs *gbs_open(char *name)
{
	int fd, i;
	struct stat st;
	struct gbs *gbs = malloc(sizeof(struct gbs));
	unsigned char *buf;

	memset(gbs, 0, sizeof(struct gbs));
	if ((fd = open(name, O_RDONLY)) == -1) {
		fprintf(stderr, "Could not open %s: %s\n", name, strerror(errno));
		return NULL;
	}
	fstat(fd, &st);
	gbs->buf = buf = malloc(st.st_size);
	read(fd, buf, st.st_size);
	if (buf[0] != 'G' ||
	    buf[1] != 'B' ||
	    buf[2] != 'S') {
		fprintf(stderr, "Not a GBS-File: %s\n", name);
		return NULL;
	}
	gbs->version = buf[3];
	if (gbs->version > 2) {
		fprintf(stderr, "GBS Version %d unsupported.\n", buf[3]);
		return NULL;
	}

	gbs->songs = buf[0x04];
	gbs->defaultsong = buf[0x05];
	gbs->load  = buf[0x06] + (buf[0x07] << 8);
	gbs->init  = buf[0x08] + (buf[0x09] << 8);
	gbs->play  = buf[0x0a] + (buf[0x0b] << 8);
	gbs->stack = buf[0x0c] + (buf[0x0d] << 8);
	gbs->tma = buf[0x0e];
	gbs->tmc = buf[0x0f];

	memcpy(gbs->v1strings, &buf[0x10], 32);
	memcpy(gbs->v1strings+33, &buf[0x30], 32);
	memcpy(gbs->v1strings+66, &buf[0x50], 32);
	gbs->title = gbs->v1strings;
	gbs->author = gbs->v1strings+33;
	gbs->copyright = gbs->v1strings+66;
	gbs->code = &buf[0x70];
	gbs->codelen = st.st_size - 0x70;
	gbs->filesize = st.st_size;

	gbs->subsong_info = malloc(gbs->songs * sizeof(struct gbs_subsong_info));
	memset(gbs->subsong_info, 0, gbs->songs * sizeof(struct gbs_subsong_info));
	if (gbs->version == 2) {
		unsigned char *buf2;
		int gbs_titleex;
		int gbs_authorex;
		int gbs_copyex;

		gbs->codelen = (buf[0x6e] + (buf[0x6f] << 8)) << 4;
		buf2 = gbs->exthdr = gbs->code + gbs->codelen;
		gbs->strings = gbs->exthdr + 16 + 4*gbs->songs;

		gbs->filesize = buf2[0] +
		               (buf2[1] << 8) +
		               (buf2[2] << 16) +
		               (buf2[3] << 24);
		gbs->crc = buf2[4] +
		          (buf2[5] << 8) +
		          (buf2[6] << 16) +
		          (buf2[7] << 24);
		buf2[4] = 0;
		buf2[5] = 0;
		buf2[6] = 0;
		buf2[7] = 0;
		gbs_titleex = buf2[8] + (buf2[9] << 8);
		gbs_authorex = buf2[10] + (buf2[11] << 8);
		gbs_copyex = buf2[12] + (buf2[13] << 8);
		if (gbs->filesize != st.st_size)
			fprintf(stderr, "Warning: file size does not match: %d != %ld\n", gbs->filesize, st.st_size);
		if (gbs_titleex != 0xffff)
			gbs->title = gbs->strings + gbs_titleex;
		if (gbs_authorex != 0xffff)
			gbs->author = gbs->strings + gbs_authorex;
		if (gbs_copyex != 0xffff)
			gbs->copyright = gbs->strings + gbs_copyex;

		for (i=0; i<gbs->songs; i++) {
			int ofs = buf2[16 + 4*i + 2] +
			         (buf2[16 + 4*i + 3] << 8);
			gbs->subsong_info[i].len = buf2[16 + 4*i] +
			                          (buf2[16 + 4*i + 1] << 8);
			if (ofs == 0xffff)
				gbs->subsong_info[i].title = NULL;
			else gbs->subsong_info[i].title = gbs->strings + ofs;
		}
	}

	gbs->crcnow = gbs_crc32(0, buf, gbs->filesize);

	gbs->romsize = (gbs->codelen + gbs->load + 0x3fff) & ~0x3fff;

	gbs->rom = malloc(gbs->romsize);
	memcpy(&gbs->rom[gbs->load - 0x70], buf, 0x70 + gbs->codelen);
	memcpy(&gbs->rom[0x50], playercode, sizeof(playercode));

	for (i=0; i<8; i++) {
		int addr = gbs->load + 8*i; /* jump address */
		gbs->rom[8*i]   = 0xc3; /* jr imm16 */
		gbs->rom[8*i+1] = addr & 0xff;
		gbs->rom[8*i+2] = addr >> 8;
	}
	gbs->rom[0x40] = 0xc9; /* reti */
	gbs->rom[0x48] = 0xc9; /* reti */

	close(fd);

	return gbs;
}
