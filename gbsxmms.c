/* $Id: gbsxmms.c,v 1.3 2003/08/25 00:07:21 ranma Exp $
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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#include <xmms/plugin.h>

#include "gbhw.h"
#include "gbcpu.h"

InputPlugin gbs_ip;

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

static int gbs_base;
static int gbs_init;
static int gbs_play;
static int gbs_stack;
static int gbs_songdefault;
static int gbs_songcnt;
static int gbs_subsong;

static int silencectr = 0;
static long long gbclock = 0;

static void gbs_playsong(int i)
{
	gbhw_init();

	if (i == -1) i = gbs_songdefault;
	if (i > gbs_songcnt) {
		printf("Subsong number out of range (min=1, max=%d).\n", gbs_songcnt);
		exit(1);
	}

	silencectr = 0;
	gbclock = 0;

	gbcpu_if = 0;
	gbcpu_halted = 0;
	REGS16_W(gbcpu_regs, PC, 0x0050); /* playercode entry point */
	REGS16_W(gbcpu_regs, SP, gbs_stack);
	REGS16_W(gbcpu_regs, HL, gbs_base - 0x70);
	gbcpu_regs.rn.a = i - 1;
}

static int open_gbs(char *name)
{
	int fd, j;
	unsigned char buf[0x70];
	struct stat st;
	int romsize, lastbank;
	char *rom;

	if ((fd = open(name, O_RDONLY)) == -1) {
		printf("Could not open %s: %s\n", name, strerror(errno));
		return 0;
	}
	fstat(fd, &st);
	read(fd, buf, sizeof(buf));
	if (buf[0] != 'G' ||
	    buf[1] != 'B' ||
	    buf[2] != 'S' ||
	    buf[3] != 1) {
		printf("Not a GBS-File: %s\n", name);
		return 0;
	}

	gbs_base  = buf[0x06] + (buf[0x07] << 8);
	gbs_init  = buf[0x08] + (buf[0x09] << 8);
	gbs_play  = buf[0x0a] + (buf[0x0b] << 8);
	gbs_stack = buf[0x0c] + (buf[0x0d] << 8);

	romsize = (st.st_size + gbs_base + 0x3fff) & ~0x3fff;
	lastbank = (romsize / 0x4000)-1;

	gbs_songcnt = buf[0x04];
	gbs_songdefault = buf[0x05];

	printf("Subsongs: %d.\n"
	       "Title:     \"%.32s\"\n"
	       "Author:    \"%.32s\"\n"
	       "Copyright: \"%.32s\"\n"
	       "Load address %04x.\n"
	       "Init address %04x.\n"
	       "Play address %04x.\n"
	       "Stack pointer %04x.\n"
	       "File size %08lx.\n"
	       "ROM size %08x (%d banks).\n",
	       gbs_songcnt,
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

	rom = gbhw_romalloc(romsize);
	memcpy(&rom[gbs_base - 0x70], buf, 0x70);
	read(fd, &rom[gbs_base], st.st_size - 0x70);
	memcpy(&rom[0x50], playercode, sizeof(playercode));

	for (j=0; j<8; j++) {
		int addr = gbs_base + 8*j; /* jump address */
		rom[8*j]   = 0xc3; /* jr imm16 */
		rom[8*j+1] = addr & 0xff;
		rom[8*j+2] = addr >> 8;
	}
	rom[0x40] = 0xc9; /* reti */
	rom[0x48] = 0xc9; /* reti */

	close(fd);

	return 1;
}

static void file_info_box(char *filename)
{
	gbs_ip.output->flush(0);
	gbs_subsong++;
	if (gbs_subsong > gbs_songcnt) gbs_subsong = 1;
	gbs_playsong(gbs_subsong);
}

static int stopthread = 1;

static void callback(void *buf, int len, void *priv)
{
	int time = gbclock / 4194304;

	while (gbs_ip.output->buffer_free() < len && !stopthread) usleep(10000);
	gbs_ip.output->write_audio(buf, len);
	gbs_ip.add_vis_pcm(gbs_ip.output->written_time(),
	                   FMT_S16_LE, 2, len/4, buf);


	if ((gbhw_ch[0].volume == 0 ||
	     gbhw_ch[0].master == 0) &&
	    (gbhw_ch[1].volume == 0 ||
	     gbhw_ch[1].master == 0) &&
	    (gbhw_ch[2].volume == 0 ||
	     gbhw_ch[2].master == 0) &&
	    (gbhw_ch[3].volume == 0 ||
	     gbhw_ch[3].master == 0)) {
		silencectr++;
	} else silencectr = 0;

	if (time > 2*60 || silencectr > 20) {
		file_info_box(NULL);
	}
}

static void init(void)
{
	gbhw_init();
	gbhw_setcallback(callback, NULL);
	gbhw_setrate(44100);
}

static int is_our_file(char *filename)
{
	int fd = open(filename, O_RDONLY);
	char id[4];

	read(fd, id, sizeof(id));

	close(fd);

	return (!strncmp(id, "GBS\1", sizeof(id)));
}

static pthread_t playthread;

void *playloop(void *priv)
{
	if (!gbs_ip.output->open_audio(FMT_S16_LE, 44100, 2)) {
		puts("Error opening output plugin.");
		return 0;
	}
	while (!stopthread)
		gbclock += gbhw_step();
	gbs_ip.output->close_audio();
	return 0;
}

static void play_file(char *filename)
{
	if (open_gbs(filename)) {
		gbs_subsong = gbs_songdefault;
		gbs_playsong(gbs_subsong);
		gbclock = 0;
		stopthread = 0;
		pthread_create(&playthread, 0, playloop, 0);
	}
}

static void stop(void)
{
	stopthread = 1;
	pthread_join(playthread, 0);
	gbhw_romfree();
}

static int get_time(void)
{
	if (stopthread) return -1;
	return gbs_ip.output->output_time();
}

static void cleanup(void)
{
}

static void get_song_info(char *filename, char **title, int *length)
{
	int fd = open(filename, O_RDONLY);
	char buf[0x70];

	read(fd, buf, sizeof(buf));
	close(fd);

	*title = malloc(3*32+10);
	*length = -1;

	snprintf(*title, 3*32+9, "%.32s - %.32s (%.32s)",
	         &buf[0x10], &buf[0x30], &buf[0x50]);
	title[3*32+9] = 0;
}

InputPlugin gbs_ip = {
description:	"GBS Player",
init:		init,
is_our_file:	is_our_file,
play_file:	play_file,
stop:		stop,
get_time:	get_time,
cleanup:	cleanup,
get_song_info:	get_song_info,
file_info_box:	file_info_box,
};

InputPlugin *get_iplugin_info(void)
{
	return &gbs_ip;
}
