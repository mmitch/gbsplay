/* $Id: gbsplay.c,v 1.22 2003/08/24 01:56:52 ranma Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include "gbhw.h"
#include "gbcpu.h"

static char playercode[] = {
	0x01, 0x30, 0x00,  /* 0050:  ld   bc, 0x0030 */
	0x11, 0x10, 0xff,  /* 0053:  ld   de, 0xff10 */
	0x21, 0x72, 0x00,  /* 0056:  ld   hl, 0x0072 */
	0x2a,              /* 0059:  ldi  a, [hl]    */
	0x12,              /* 005a:  ld   [de], a    */
	0x13,              /* 005b:  inc  de         */
	0x0b,              /* 005c:  dec  bc         */
	0x78,              /* 005d:  ld   a, b       */
	0xb1,              /* 005e:  or   a, c       */
	0x20, 0xf8,        /* 005f:  jr nz $-0x08    */
	0x1e, 0xff,        /* 0061:  ld   e, 0xff    */
	0x3e, 0x06,        /* 0063:  ld   a, 0x06    */
	0x12,              /* 0065:  ld   [de], a    */
	0x3e, 0x00,        /* 0066:  ld   a, 0x00    */
	0xcd, 0x00, 0x00,  /* 0068:  call init       */
	0xfb,              /* 006b:  ei              */
	0x76,              /* 006c:  halt            */
	0xcd, 0x00, 0x00,  /* 006d:  call play       */
	0x18, 0xfa,        /* 0070:  jr $-6          */

	0x80, 0xbf, 0x00, 0x00, 0xbf, /* 0072: initdata */
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

static void open_gbs(char *name, int i)
{
	int fd, j;
	unsigned char buf[0x70];
	struct stat st;
	int gbs_base, gbs_init, gbs_play, gbs_stack;
	int romsize, lastbank;
	char *rom;

	if ((fd = open(name, O_RDONLY)) == -1) {
		printf("Could not open %s: %s\n", name, strerror(errno));
		exit(1);
	}
	fstat(fd, &st);
	read(fd, buf, sizeof(buf));
	if (buf[0] != 'G' ||
	    buf[1] != 'B' ||
	    buf[2] != 'S' ||
	    buf[3] != 1) {
		printf("Not a GBS-File: %s\n", name);
		exit(1);
	}

	gbs_base  = buf[0x06] + (buf[0x07] << 8);
	gbs_init  = buf[0x08] + (buf[0x09] << 8);
	gbs_play  = buf[0x0a] + (buf[0x0b] << 8);
	gbs_stack = buf[0x0c] + (buf[0x0d] << 8);

	romsize = (st.st_size + gbs_base + 0x3fff) & ~0x3fff;
	lastbank = (romsize / 0x4000)-1;

	if (i == -1) i = buf[0x5];
	if (i > buf[0x04]) {
		printf("Subsong number out of range (min=1, max=%d).\n", buf[0x4]);
		exit(1);
	}

	printf("Playing song %d/%d.\n"
	       "Title:     \"%.32s\"\n"
	       "Author:    \"%.32s\"\n"
	       "Copyright: \"%.32s\"\n"
	       "Load address %04x.\n"
	       "Init address %04x.\n"
	       "Play address %04x.\n"
	       "Stack pointer %04x.\n"
	       "File size %08lx.\n"
	       "ROM size %08x (%d banks).\n",
	       i, buf[0x04],
	       &buf[0x10],
	       &buf[0x30],
	       &buf[0x50],
	       gbs_base,
	       gbs_init,
	       gbs_play,
	       gbs_stack,
	       st.st_size,
	       romsize,
	       romsize/0x4000);


/*	if (buf[0x0f] & 4) {
		ioregs[0x06] = buf[0x0e];
		ioregs[0x07] = buf[0x0f];
		timertc = (256-buf[0x0e]) * (16 << (((buf[0x0f]+3) & 3) << 1));
		if ((buf[0x0f] & 0xf0) == 0x80) timertc /= 2;
		printf("Callback rate %2.2fHz (Custom).\n", 4194304/(float)timertc);
	} else {
		timertc = 70256;
		printf("Callback rate %2.2fHz (VBlank).\n", 4194304/(float)timertc);
	}*/

	rom = gbhw_romalloc(romsize);
	memcpy(&rom[gbs_base - 0x70], buf, 0x70);
	read(fd, &rom[gbs_base], st.st_size - 0x70);

	for (j=0; j<8; j++) {
		int addr = gbs_base + 8*j; /* jump address */
		rom[8*j]   = 0xc3; /* jr imm16 */
		rom[8*j+1] = addr & 0xff;
		rom[8*j+2] = addr >> 8;
	}
	rom[0x40] = 0xc9; /* reti */
	rom[0x48] = 0xc9; /* reti */

	memcpy(&rom[0x50], playercode, sizeof(playercode));

	rom[0x67] = i - 1;
	rom[0x69] = gbs_init & 0xff;
	rom[0x6a] = gbs_init >> 8;
	rom[0x6e] = gbs_play & 0xff;
	rom[0x6f] = gbs_play >> 8;

	REGS16_W(regs, PC, 0x0050); /* playercode entry point */
	REGS16_W(regs, SP, gbs_stack);

	close(fd);
}

#define LN2 .69314718055994530941
#define MAGIC 6.02980484763069750723
#define FREQ(x) (262144 / x)
// #define NOTE(x) ((log(FREQ(x))/LN2 - log(65.33593)/LN2)*12 + .2)
#define NOTE(x) ((int)((log(FREQ(x))/LN2 - MAGIC)*12 + .2))

#define MAXOCTAVE 9

static int getnote(int div)
{
	int n = 0;

	if (div>0) n = NOTE(div);

	if (n < 0) n = 0;
	else if (n >= MAXOCTAVE*12) n = MAXOCTAVE-1;

	return n;
}

static char notes[7] = "CDEFGAH";
static char notelookup[4*MAXOCTAVE*12];
static void precalc_notes(void)
{
	int i;
	for (i=0; i<MAXOCTAVE*12; i++) {
		char *s = notelookup + 4*i;
		int n = i % 12;

		s[2] = '0' + i / 12;
		n += n > 4;
		s[0] = notes[n >> 1];
		if (n & 1) s[1] = '#';
		else s[1] = '-';
	}
}

static char vols[5] = " -=#%";
static char vollookup[5*16];
static void precalc_vols(void)
{
	int i, k;
	for (k=0; k<16; k++) {
		int j;
		char *s = vollookup + 5*k;
		i = k;
		for (j=0; j<4; j++) {
			if (i>=4) {
				s[j] = vols[4];
				i -= 4;
			} else {
				s[j] = vols[i];
				i = 0;
			}
		}
	}
}

static int statustc = 83886;
static int statuscnt;

static int dspfd;

void callback(void *buf, int len, void *priv)
{
	write(dspfd, buf, len);
}

int main(int argc, char **argv)
{
	precalc_notes();
	precalc_vols();

	dspfd = open("/dev/dsp", O_WRONLY);
	int c;
	int subsong = -1;

	c=AFMT_S16_LE;
	ioctl(dspfd, SNDCTL_DSP_SETFMT, &c);
	c=1;
	ioctl(dspfd, SNDCTL_DSP_STEREO, &c);
	c=44100;
	ioctl(dspfd, SNDCTL_DSP_SPEED, &c);
	c=(4 << 16) + 11;
	ioctl(dspfd, SNDCTL_DSP_SETFRAGMENT, &c);
	
	gbhw_init();
	gbhw_setcallback(callback, NULL);
	gbhw_setrate(44100);
	
	if (argc < 2) {
		printf("Usage: %s <gbs-file> [<subsong>]\n", argv[0]);
		exit(1);
	}
	if (argc == 3) sscanf(argv[2], "%d", &subsong);
	open_gbs(argv[1], subsong);
	while (1) {
		int cycles = gbhw_step();
		statuscnt -= cycles;
		if (statuscnt < 0) {
			int ni1 = getnote(gbhw_ch[1].div_tc);
			int ni2 = getnote(gbhw_ch[2].div_tc);
			int ni3 = getnote(gbhw_ch[3].div_tc);
			char *n1 = &notelookup[4*ni1];
			char *n2 = &notelookup[4*ni2];
			char *n3 = &notelookup[4*ni3];
			char *v1 = &vollookup[5*gbhw_ch[1].volume];
			char *v2 = &vollookup[5*gbhw_ch[2].volume];
			char *v3 = &vollookup[5*((3-((gbhw_ch[3].volume+3)&3)) << 2)];
			char *v4 = &vollookup[5*gbhw_ch[4].volume];

			statuscnt += statustc;

			if (!gbhw_ch[1].volume) n1 = "---";
			if (!gbhw_ch[2].volume) n2 = "---";
			if (!gbhw_ch[3].volume) n3 = "---";

			printf("ch1: %s %s  ch2: %s %s  ch3: %s %s  ch4: %s\r",
				n1, v1, n2, v2, n3, v3, v4);
			fflush(stdout);
		}
	}
	return 0;
}
