/*
 * gbsplay is a Gameboy sound player
 *
 * 2003-2006,2013 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                       Christian Garbs <mitch@cgarbs.de>
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
#include <ctype.h>

#include "common.h"
#include "gbhw.h"
#include "gbcpu.h"
#include "gbs.h"
#include "crc32.h"

#define GBS_MAGIC		"GBS"
#define GBS_EXTHDR_MAGIC	"GBSX"
#define GBR_MAGIC		"GBRF"

regparm long gbs_init(struct gbs *gbs, long subsong)
{
	gbhw_init(gbs->rom, gbs->romsize);

	if (subsong == -1) subsong = gbs->defaultsong - 1;
	if (subsong >= gbs->songs) {
		fprintf(stderr, _("Subsong number out of range (min=0, max=%d).\n"), (int)gbs->songs - 1);
		return 0;
	}

	if (gbs->defaultbank != 1) {
		gbcpu_mem_put(0x2000, gbs->defaultbank);
	}
	gbhw_io_put(0xff06, gbs->tma);
	gbhw_io_put(0xff07, gbs->tac);
	gbhw_io_put(0xffff, 0x05);

	REGS16_W(gbcpu_regs, SP, gbs->stack);

	/* put halt breakpoint PC on stack */
	gbcpu_halt_at_pc = 0xffff;
	REGS16_W(gbcpu_regs, PC, 0xff80);
	REGS16_W(gbcpu_regs, HL, gbcpu_halt_at_pc);
	gbcpu_mem_put(0xff80, 0xe5); /* push hl */
	gbcpu_step();
	/* clear regs/memory touched by stack etup */
	REGS16_W(gbcpu_regs, HL, 0x0000);
	gbcpu_mem_put(0xff80, 0x00);

	REGS16_W(gbcpu_regs, PC, gbs->init);
	gbcpu_regs.rn.a = subsong;

	gbs->ticks = 0;
	gbs->subsong = subsong;

	return 1;
}

regparm void gbs_set_nextsubsong_cb(struct gbs *gbs, gbs_nextsubsong_cb cb, void *priv)
{
	gbs->nextsubsong_cb = cb;
	gbs->nextsubsong_cb_priv = priv;
}

static regparm long gbs_nextsubsong(struct gbs *gbs)
{
	if (gbs->nextsubsong_cb != NULL) {
		return gbs->nextsubsong_cb(gbs, gbs->nextsubsong_cb_priv);
	} else {
		gbs->subsong++;
		if (gbs->subsong >= gbs->songs)
			return false;
		gbs_init(gbs, gbs->subsong);
	}
	return true;
}

regparm long gbs_step(struct gbs *gbs, long time_to_work)
{
	long cycles = gbhw_step(time_to_work);
	long time;

	if (cycles < 0) {
		return false;
	}

	gbs->ticks += cycles;

	gbhw_getminmax(&gbs->lmin, &gbs->lmax, &gbs->rmin, &gbs->rmax);
	gbs->lvol = -gbs->lmin > gbs->lmax ? -gbs->lmin : gbs->lmax;
	gbs->rvol = -gbs->rmin > gbs->rmax ? -gbs->rmin : gbs->rmax;

	time = gbs->ticks / GBHW_CLOCK;
	if (gbs->silence_timeout) {
		if (gbs->lmin == gbs->lmax && gbs->rmin == gbs->rmax) {
			if (gbs->silence_start == 0)
				gbs->silence_start = gbs->ticks;
		} else gbs->silence_start = 0;
	}

	if (gbs->fadeout && gbs->subsong_timeout &&
	    time >= gbs->subsong_timeout - gbs->fadeout - gbs->gap)
		gbhw_master_fade(128/gbs->fadeout, 0);
	if (gbs->subsong_timeout &&
	    time >= gbs->subsong_timeout - gbs->gap)
		gbhw_master_fade(128*16, 0);

	if (gbs->silence_start &&
	    (gbs->ticks - gbs->silence_start) / GBHW_CLOCK >= gbs->silence_timeout) {
		if (gbs->subsong_info[gbs->subsong].len == 0) {
			gbs->subsong_info[gbs->subsong].len = gbs->ticks * GBS_LEN_DIV / GBHW_CLOCK;
		}
		return gbs_nextsubsong(gbs);
	}
	if (gbs->subsong_timeout && time >= gbs->subsong_timeout)
		return gbs_nextsubsong(gbs);

	return true;
}

regparm void gbs_printinfo(struct gbs *gbs, long verbose)
{
	printf(_("GBSVersion:       %u\n"
	         "Title:            \"%s\"\n"
	         "Author:           \"%s\"\n"
	         "Copyright:        \"%s\"\n"
	         "Load address:     0x%04x\n"
	         "Init address:     0x%04x\n"
	         "Play address:     0x%04x\n"
	         "Stack pointer:    0x%04x\n"
	         "File size:        0x%08x\n"
	         "ROM size:         0x%08lx (%ld banks)\n"
	         "Subsongs:         %u\n"
	         "Default subsong:  %u\n"),
	       gbs->version,
	       gbs->title,
	       gbs->author,
	       gbs->copyright,
	       gbs->load,
	       gbs->init,
	       gbs->play,
	       gbs->stack,
	       (unsigned int)gbs->filesize,
	       gbs->romsize,
	       gbs->romsize/0x4000,
	       gbs->songs,
	       gbs->defaultsong);
	if (gbs->tac & 0x04) {
		long timertc = (256-gbs->tma) * (16 << (((gbs->tac+3) & 3) << 1));
		if (gbs->tac & 0x80)
			timertc /= 2;
		printf(_("Timing:           %2.2fHz timer%s\n"),
		       GBHW_CLOCK / (float)timertc,
		       (gbs->tac & 0x78) == 0x40 ? _(" + VBlank (ugetab)") : "");
	} else {
		printf(_("Timing:           %s\n"),
		       _("59.7Hz VBlank\n"));
	}
	if (gbs->defaultbank != 1) {
		printf(_("Bank @0x4000:     %d\n"), gbs->defaultbank);
	}
	if (gbs->version == 2) {
		printf(_("CRC32:            0x%08lx/0x%08lx (%s)\n"),
		       (unsigned long)gbs->crc, (unsigned long)gbs->crcnow,
		       gbs->crc == gbs->crcnow ? _("OK") : _("Failed"));
	} else {
		printf(_("CRC32:            0x%08lx\n"),
		       (unsigned long)gbs->crcnow);
	}
	if (verbose && gbs->version == 2) {
		long i;
		for (i=0; i<gbs->songs; i++) {
			printf(_("Subsong %03ld:	"), i);
			if (gbs->subsong_info[i].title) {
				printf("\"%s\" ", gbs->subsong_info[i].title);
			} else {
				printf("%s ", _("untitled"));
			}
			if (gbs->subsong_info[i].len) {
				printf(_("(%ld seconds)\n"),
				       (long)(gbs->subsong_info[i].len >> GBS_LEN_SHIFT));
			} else {
				printf("%s\n", _("no time limit"));
			}
		}
	}
}

static regparm void gbs_free(struct gbs *gbs)
{
	if (gbs->buf)
		free(gbs->buf);
	if (gbs->subsong_info)
		free(gbs->subsong_info);
	free(gbs);
}

regparm void gbs_close(struct gbs *gbs)
{
	gbs_free(gbs);
}

static regparm void writeint(char *buf, uint32_t val, long bytes)
{
	long shift = 0;
	long i;

	for (i=0; i<bytes; i++) {
		buf[i] = (val >> shift) & 0xff;
		shift += 8;
	}
}

static regparm uint32_t readint(char *buf, long bytes)
{
	long i;
	long shift = 0;
	uint32_t res = 0;

	for (i=0; i<bytes; i++) {
		res |= (unsigned char)buf[i] << shift;
		shift += 8;
	}

	return res;
}

regparm long gbs_write(struct gbs *gbs, char *name, long version)
{
	long fd;
	long codelen = (gbs->codelen + 15) >> 4;
	char pad[16];
	char strings[65536];
	long stringofs = 0;
	long newlen = gbs->filesize;
	long namelen = strlen(name);
	char *tmpname = malloc(namelen + sizeof(".tmp\0"));

	memcpy(tmpname, name, namelen);
	sprintf(&tmpname[namelen], ".tmp");
	memset(pad, 0xff, sizeof(pad));

	if ((fd = open(tmpname, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1) {
		fprintf(stderr, _("Could not open %s: %s\n"), name, strerror(errno));
		return 0;
	}

	if (version == 2) {
		long len,i;
		long ehdrlen = 32 + 8*gbs->songs;
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
	if (write(fd, gbs->buf, newlen) == newlen) {
		int ret = 1;
		close(fd);
		if (rename(tmpname, name) == -1) {
			fprintf(stderr, _("Could not rename %s to %s: %s\n"), tmpname, name, strerror(errno));
			ret = 0;
		}
		return ret;
	}
	close(fd);

	return 1;
}

static regparm struct gbs *gbr_open(char *name)
{
	long fd, i;
	struct stat st;
	struct gbs *gbs = malloc(sizeof(struct gbs));
	char *buf;
	char *na_str = _("gbr / not available");
	uint16_t vsync_addr;
	uint16_t timer_addr;

	memset(gbs, 0, sizeof(struct gbs));
	gbs->silence_timeout = 2;
	gbs->subsong_timeout = 2*60;
	gbs->gap = 2;
	gbs->fadeout = 3;
	if ((fd = open(name, O_RDONLY)) == -1) {
		fprintf(stderr, _("Could not open %s: %s\n"), name, strerror(errno));
		gbs_free(gbs);
		return NULL;
	}
	fstat(fd, &st);
	gbs->buf = buf = malloc(st.st_size);
	if (read(fd, buf, st.st_size) != st.st_size) {
		fprintf(stderr, _("Could not read %s: %s\n"), name, strerror(errno));
		gbs_free(gbs);
		return NULL;
	}
	if (strncmp(buf, GBR_MAGIC, 4) != 0) {
		fprintf(stderr, _("Not a GBR-File: %s\n"), name);
		gbs_free(gbs);
		return NULL;
	}
	if (buf[0x05] != 0) {
		fprintf(stderr, _("Unsupported default bank @0x0000: %d\n"), buf[0x05]);
		gbs_free(gbs);
		return NULL;
	}
	if (buf[0x07] < 1 || buf[0x07] > 3) {
		fprintf(stderr, _("Unsupported timerflag value: %d\n"), buf[0x07]);
		gbs_free(gbs);
		return NULL;
	}
	gbs->version = 0;
	gbs->songs = 255;
	gbs->defaultsong = 1;
	gbs->defaultbank = buf[0x06];
	gbs->load  = 0;
	gbs->init  = readint(&buf[0x08], 2);
	vsync_addr = readint(&buf[0x0a], 2);
	timer_addr = readint(&buf[0x0c], 2);

	if (buf[0x07] == 1) {
		gbs->play = vsync_addr;
	} else {
		gbs->play = timer_addr;
	}
	gbs->tma = buf[0x0e];
	gbs->tac = buf[0x0f];
	gbs->stack = 0xfffe;

	/* Test if this looks like a valid rom header title */
	for (i=0x0154; i<0x0163; i++) {
		if (!(isalnum(buf[i]) || isspace(buf[i])))
			break;
	}
	if (buf[i] == 0) {
		/* Title looks valid and is zero-terminated. */
		gbs->title = &buf[0x0154];
	} else {
		gbs->title = na_str;
	}
	gbs->author = na_str;
	gbs->copyright = na_str;
	gbs->code = &buf[0x20];
	gbs->filesize = st.st_size;

	gbs->subsong_info = malloc(gbs->songs * sizeof(struct gbs_subsong_info));
	memset(gbs->subsong_info, 0, gbs->songs * sizeof(struct gbs_subsong_info));
	gbs->codelen = st.st_size - 0x20;
	gbs->crcnow = gbs_crc32(0, buf, gbs->filesize);
	gbs->romsize = (gbs->codelen + 0x3fff) & ~0x3fff;

	gbs->rom = calloc(1, gbs->romsize);
	memcpy(gbs->rom, &buf[0x20], gbs->codelen);

	gbs->rom[0x40] = 0xc9; /* reti */
	gbs->rom[0x50] = 0xc9; /* reti */
	if (buf[0x07] & 1) {
		/* V-Blank */
		gbs->rom[0x40] = 0xc3; /* jp imm16 */
		gbs->rom[0x41] = vsync_addr & 0xff;
		gbs->rom[0x42] = vsync_addr >> 8;
	}
	if (buf[0x07] & 2) {
		/* Timer */
		gbs->rom[0x50] = 0xc3; /* jp imm16 */
		gbs->rom[0x51] = timer_addr & 0xff;
		gbs->rom[0x52] = timer_addr >> 8;
	}
	close(fd);

	return gbs;
}

regparm struct gbs *gbs_open(char *name)
{
	long fd, i;
	struct stat st;
	struct gbs *gbs = malloc(sizeof(struct gbs));
	char *buf;
	char *buf2 = NULL;
	long have_ehdr = 0;

	memset(gbs, 0, sizeof(struct gbs));
	gbs->silence_timeout = 2;
	gbs->subsong_timeout = 2*60;
	gbs->gap = 2;
	gbs->fadeout = 3;
	gbs->defaultbank = 1;
	if ((fd = open(name, O_RDONLY)) == -1) {
		fprintf(stderr, _("Could not open %s: %s\n"), name, strerror(errno));
		gbs_free(gbs);
		return NULL;
	}
	fstat(fd, &st);
	gbs->buf = buf = malloc(st.st_size);
	if (read(fd, buf, st.st_size) != st.st_size) {
		fprintf(stderr, _("Could not read %s: %s\n"), name, strerror(errno));
		gbs_free(gbs);
		return NULL;
	}
	if (strncmp(buf, GBR_MAGIC, 4) == 0) {
		gbs_free(gbs);
		return gbr_open(name);
	}
	if (strncmp(buf, GBS_MAGIC, 3) != 0) {
		fprintf(stderr, _("Not a GBS-File: %s\n"), name);
		gbs_free(gbs);
		return NULL;
	}
	gbs->version = buf[0x03];
	if (gbs->version != 1) {
		fprintf(stderr, _("GBS Version %d unsupported.\n"), gbs->version);
		gbs_free(gbs);
		return NULL;
	}

	gbs->songs = buf[0x04];
	if (gbs->songs < 1) {
		fprintf(stderr, _("Number of subsongs = %d is unreasonable.\n"), gbs->songs);
		gbs_free(gbs);
		return NULL;
	}

	gbs->defaultsong = buf[0x05];
	if (gbs->defaultsong < 1 || gbs->defaultsong > gbs->songs) {
		fprintf(stderr, _("Default subsong %d is out of range [1..%d].\n"), gbs->defaultsong, gbs->songs);
		gbs_free(gbs);
		return NULL;
	}

	gbs->load  = readint(&buf[0x06], 2);
	gbs->init  = readint(&buf[0x08], 2);
	gbs->play  = readint(&buf[0x0a], 2);
	gbs->stack = readint(&buf[0x0c], 2);
	gbs->tma = buf[0x0e];
	gbs->tac = buf[0x0f];

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
			fprintf(stderr, _("Warning: Extended header found, but CRC does not match (0x%08x != 0x%08x).\n"),
			        crc, realcrc);
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
		long gbs_titleex;
		long gbs_authorex;
		long gbs_copyex;
		long entries;

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
			long ofs = readint(&buf2[32 + 8*i + 4], 2);
			gbs->subsong_info[i].len = readint(&buf2[32 + 8*i], 4);
			if (ofs == 0xffff)
				gbs->subsong_info[i].title = NULL;
			else gbs->subsong_info[i].title = gbs->strings + ofs;
		}

		if (gbs->crc != gbs->crcnow) {
			fprintf(stderr, _("Warning: File CRC does not match (0x%08x != 0x%08x).\n"),
			        gbs->crc, gbs->crcnow);
		}
	}

	gbs->romsize = (gbs->codelen + gbs->load + 0x3fff) & ~0x3fff;

	gbs->rom = calloc(1, gbs->romsize);
	memcpy(&gbs->rom[gbs->load - 0x70], buf, 0x70 + gbs->codelen);

	for (i=0; i<8; i++) {
		long addr = gbs->load + 8*i; /* jump address */
		gbs->rom[8*i]   = 0xc3; /* jp imm16 */
		gbs->rom[8*i+1] = addr & 0xff;
		gbs->rom[8*i+2] = addr >> 8;
	}
	if ((gbs->tac & 0x78) == 0x40) { /* ugetab int vector extension */
		/* V-Blank */
		gbs->rom[0x40] = 0xc3; /* jp imm16 */
		gbs->rom[0x41] = (gbs->load + 0x40) & 0xff;
		gbs->rom[0x42] = (gbs->load + 0x40) >> 8;
		/* Timer */
		gbs->rom[0x50] = 0xc3; /* jp imm16 */
		gbs->rom[0x51] = (gbs->load + 0x48) & 0xff;
		gbs->rom[0x52] = (gbs->load + 0x48) >> 8;
	} else if (gbs->tac & 0x04) { /* timer enabled */
		/* V-Blank */
		gbs->rom[0x40] = 0xc9; /* reti */
		/* Timer */
		gbs->rom[0x50] = 0xc3; /* jp imm16 */
		gbs->rom[0x51] = gbs->play & 0xff;
		gbs->rom[0x52] = gbs->play >> 8;
	} else {
		/* V-Blank */
		gbs->rom[0x40] = 0xc3; /* jp imm16 */
		gbs->rom[0x41] = gbs->play & 0xff;
		gbs->rom[0x42] = gbs->play >> 8;
		/* Timer */
		gbs->rom[0x50] = 0xc9; /* reti */
	}
	gbs->rom[0x48] = 0xc9; /* reti (LCD Stat) */
	gbs->rom[0x58] = 0xc9; /* reti (Serial) */
	gbs->rom[0x60] = 0xc9; /* reti (Joypad) */

	close(fd);

	return gbs;
}
