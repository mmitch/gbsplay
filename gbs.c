/* $Id: gbs.c,v 1.13 2003/12/05 22:02:19 mitch Exp $
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

#define GBS_MAGIC		"GBS"
#define GBS_EXTHDR_MAGIC	"GBSX"

static const uint8_t playercode[] = {
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
	printf("GBSVersion:     %d\n"
	       "Title:          \"%s\"\n"
	       "Author:         \"%s\"\n"
	       "Copyright:      \"%s\"\n"
	       "Load address:   0x%04x\n"
	       "Init address:   0x%04x\n"
	       "Play address:   0x%04x\n"
	       "Stack pointer:  0x%04x\n"
	       "File size:      0x%08x\n"
	       "ROM size:       0x%08x (%d banks)\n"
	       "Subsongs:       %d\n",
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
		printf("CRC32:		0x%08x/0x%08x (%s)\n",
		       (unsigned int)gbs->crc, (unsigned int)gbs->crcnow,
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
				printf("(%d seconds)\n",
				       (int)(gbs->subsong_info[i].len >> GBS_LEN_SHIFT));
			} else {
				printf("(no timelimit)\n");
			}
		}
	}
}

void gbs_close(struct gbs *gbs)
{
	free(gbs->subsong_info);
	free(gbs);
}

void writeint(uint8_t *buf, uint32_t val, int bytes)
{
	int shift = 0;
	int i;

	for (i=0; i<bytes; i++) {
		buf[i] = (val >> shift) & 0xff;
		shift += 8;
	}
}

uint32_t readint(uint8_t *buf, int bytes)
{
	int i;
	int shift = 0;
	uint32_t res = 0;

	for (i=0; i<bytes; i++) {
		res |= buf[i] << shift;
		shift += 8;
	}

	return res;
}

int gbs_write(struct gbs *gbs, char *name, int version)
{
	int fd;
	int codelen = (gbs->codelen + 15) >> 4;
	char pad[16];
	char strings[65536];
	int stringofs = 0;
	int newlen = gbs->filesize;

	memset(pad, 0xff, sizeof(pad));

	if ((fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1) {
		fprintf(stderr, "Could not open %s: %s\n", name, strerror(errno));
		return 0;
	}

	if (version == 2) {
		int len,i;
		int ehdrlen = 32 + 8*gbs->songs;
		uint32_t hdrcrc;

		newlen = 0x70 + codelen*16 + ehdrlen;
		gbs->buf[3] = 1;
		gbs->buf = realloc(gbs->buf, newlen + 65536);
		gbs->code = gbs->buf + 0x70;
		gbs->exthdr = gbs->code + codelen*16;
		writeint(gbs->buf + 0x6e, codelen, 2);
		memset(&gbs->code[gbs->codelen], 0x00, codelen*16 - gbs->codelen);
		memset(gbs->exthdr, 0x00, ehdrlen + 65536);
		strncpy(gbs->exthdr, GBS_EXTHDR_MAGIC, 4);
		gbs->exthdr[0x1c] = gbs->songs;
		if ((len = strlen(gbs->title)) > 32) {
			memcpy(strings+stringofs, gbs->title, len+1);
			writeint(&gbs->exthdr[0x14], stringofs, 2);
			stringofs += len+1;
		} else writeint(&gbs->exthdr[0x14], 0xffff, 2);
		if ((len = strlen(gbs->author)) > 32) {
			memcpy(strings+stringofs, gbs->author, len+1);
			writeint(&gbs->exthdr[0x16], stringofs, 2);
			stringofs += len+1;
		} else writeint(&gbs->exthdr[0x16], 0xffff, 2);
		if ((len = strlen(gbs->copyright)) > 30) {
			memcpy(strings+stringofs, gbs->copyright, len+1);
			writeint(&gbs->exthdr[0x18], stringofs, 2);
			stringofs += len+1;
		} else writeint(&gbs->exthdr[0x18], 0xffff, 2);

		for (i=0; i<gbs->songs; i++) {
			writeint(&gbs->exthdr[0x20+8*i],
			         gbs->subsong_info[i].len, 4);
			if (gbs->subsong_info[i].title &&
			    strcmp(gbs->subsong_info[i].title, "") != 0) {
				len = strlen(gbs->subsong_info[i].title)+1;
				memcpy(strings+stringofs, gbs->subsong_info[i].title, len);
				writeint(&gbs->exthdr[0x20+8*i+4],
				         stringofs, 2);
				stringofs += len;
			} else writeint(&gbs->exthdr[0x20+8*i+4], 0xffff, 2);
		}
		memcpy(gbs->buf + newlen, strings, stringofs);
		newlen += stringofs;

		writeint(&gbs->exthdr[0x04], ehdrlen+stringofs-8, 4);
		writeint(&gbs->exthdr[0x0c], gbs->filesize, 4);
		gbs->crc = gbs_crc32(0, gbs->buf, gbs->filesize);
		writeint(&gbs->exthdr[0x10], gbs->crc, 4);
		hdrcrc = gbs_crc32(0, gbs->exthdr, ehdrlen+stringofs);
		writeint(&gbs->exthdr[0x08], hdrcrc, 4);

	} else {
		if (gbs->version == 2) {
			gbs->buf[3] = 1;
		}
	}
	write(fd, gbs->buf, newlen);
	close(fd);

	return 1;
}

struct gbs *gbs_open(char *name)
{
	int fd, i;
	struct stat st;
	struct gbs *gbs = malloc(sizeof(struct gbs));
	uint8_t *buf;
	uint8_t *buf2 = NULL;
	int have_ehdr = 0;

	memset(gbs, 0, sizeof(struct gbs));
	if ((fd = open(name, O_RDONLY)) == -1) {
		fprintf(stderr, "Could not open %s: %s\n", name, strerror(errno));
		return NULL;
	}
	fstat(fd, &st);
	gbs->buf = buf = malloc(st.st_size);
	read(fd, buf, st.st_size);
	if (strncmp(buf, GBS_MAGIC, 3) != 0) {
		fprintf(stderr, "Not a GBS-File: %s\n", name);
		return NULL;
	}
	gbs->version = buf[3];
	if (gbs->version != 1) {
		fprintf(stderr, "GBS Version %d unsupported.\n", buf[3]);
		return NULL;
	}

	gbs->songs = buf[0x04];
	gbs->defaultsong = buf[0x05];
	gbs->load  = readint(&buf[0x06], 2);
	gbs->init  = readint(&buf[0x08], 2);
	gbs->play  = readint(&buf[0x0a], 2);
	gbs->stack = readint(&buf[0x0c], 2);
	gbs->tma = buf[0x0e];
	gbs->tmc = buf[0x0f];

	memcpy(gbs->v1strings, &buf[0x10], 32);
	memcpy(gbs->v1strings+33, &buf[0x30], 32);
	memcpy(gbs->v1strings+66, &buf[0x50], 30);
	gbs->title = gbs->v1strings;
	gbs->author = gbs->v1strings+33;
	gbs->copyright = gbs->v1strings+66;
	gbs->code = &buf[0x70];
	gbs->filesize = st.st_size;

	gbs->subsong_info = malloc(gbs->songs * sizeof(struct gbs_subsong_info));
	memset(gbs->subsong_info, 0, gbs->songs * sizeof(struct gbs_subsong_info));
	gbs->codelen = (buf[0x6e] + (buf[0x6f] << 8)) << 4;
	if ((0x70 + gbs->codelen) < (gbs->filesize - 8) &&
	    strncmp(&buf[0x70 + gbs->codelen], GBS_EXTHDR_MAGIC, 4) == 0) {
		uint32_t crc, realcrc, ehdrlen;

		gbs->exthdr = gbs->code + gbs->codelen;
		ehdrlen = readint(&gbs->exthdr[0x04], 4) + 8;
		crc = readint(&gbs->exthdr[0x08], 4);
		writeint(&gbs->exthdr[0x08], 0, 4);

		if ((realcrc=gbs_crc32(0, gbs->exthdr, ehdrlen)) == crc) {
			have_ehdr = 1;
		} else {
			fprintf(stderr, "Warning: Extended header found, but CRC does not match (0x%08x != 0x%08x).\n",
			        (unsigned int)crc, (unsigned int)realcrc);
		}
	}
	if (have_ehdr) {
		buf2 = gbs->exthdr;
		gbs->filesize = readint(&buf2[0x0c], 4);
		gbs->crc = readint(&buf2[0x10], 4);
		writeint(&buf2[0x10], 0, 4);
	} else {
		memcpy(gbs->v1strings+66, &buf[0x50], 32);
		gbs->codelen = st.st_size - 0x70;
	}
	gbs->crcnow = gbs_crc32(0, buf, gbs->filesize);
	if (have_ehdr) {
		int gbs_titleex;
		int gbs_authorex;
		int gbs_copyex;
		int entries;

		gbs->version = 2;
		entries = buf2[0x1c];
		gbs->strings = gbs->exthdr + 32 + 8*entries;

		gbs_titleex = readint(&buf2[0x14], 2);
		gbs_authorex = readint(&buf2[0x16], 2);
		gbs_copyex = readint(&buf2[0x18], 2);
		if (gbs_titleex != 0xffff)
			gbs->title = gbs->strings + gbs_titleex;
		if (gbs_authorex != 0xffff)
			gbs->author = gbs->strings + gbs_authorex;
		if (gbs_copyex != 0xffff)
			gbs->copyright = gbs->strings + gbs_copyex;

		for (i=0; i<entries; i++) {
			int ofs = readint(&buf2[32 + 8*i + 4], 2);
			gbs->subsong_info[i].len = readint(&buf2[32 + 8*i], 4);
			if (ofs == 0xffff)
				gbs->subsong_info[i].title = NULL;
			else gbs->subsong_info[i].title = gbs->strings + ofs;
		}

		if (gbs->crc != gbs->crcnow) {
			fprintf(stderr, "Warning: File CRC does not match (0x%08x != 0x%08x).\n",
			        (unsigned int)gbs->crc, (unsigned int)gbs->crcnow);
		}
	}

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
