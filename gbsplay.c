/* $Id: gbsplay.c,v 1.36 2003/08/27 15:07:03 ranma Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include "gbhw.h"
#include "gbcpu.h"
#include "gbs.h"

#define LN2 .69314718055994530941
#define MAGIC 5.78135971352465960412
#define FREQ(x) (262144 / x)
// #define NOTE(x) ((log(FREQ(x))/LN2 - log(55)/LN2)*12 + .2)
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

static char notelookup[4*MAXOCTAVE*12];
static void precalc_notes(void)
{
	int i;
	for (i=0; i<MAXOCTAVE*12; i++) {
		char *s = notelookup + 4*i;
		int n = i % 12;

		s[2] = '0' + i / 12;
		n += (n > 2) + (n > 7);
		s[0] = 'A' + (n >> 1);
		if (n & 1) s[1] = '#';
		else s[1] = '-';
	}
}

static const char vols[5] = " -=#%";
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

void open_dsp(void)
{
	int c;

	if ((dspfd = open("/dev/dsp", O_WRONLY)) == -1) {
		printf("Could not open /dev/dsp: %s\n", strerror(errno));
		exit(1);
	}

	c=AFMT_S16_LE;
	if ((ioctl(dspfd, SNDCTL_DSP_SETFMT, &c)) == -1) {
		printf("ioctl(dspfd, SNDCTL_DSP_SETFMT, %d) failed: %s\n", c, strerror(errno));
		exit(1);
	}
	c=1;
	if ((ioctl(dspfd, SNDCTL_DSP_STEREO, &c)) == -1) {
		printf("ioctl(dspfd, SNDCTL_DSP_STEREO, %d) failed: %s\n", c, strerror(errno));
		exit(1);
	}
	c=44100;
	if ((ioctl(dspfd, SNDCTL_DSP_SPEED, &c)) == -1) {
		printf("ioctl(dspfd, SNDCTL_DSP_SPEED, %d) failed: %s\n", c, strerror(errno));
		exit(1);
	}
	c=(4 << 16) + 11;
	if ((ioctl(dspfd, SNDCTL_DSP_SETFRAGMENT, &c)) == -1) {
		printf("ioctl(dspfd, SNDCTL_DSP_SETFRAGMENT, %d) failed: %s\n", c, strerror(errno));
		exit(1);
	}
	
}

static int timeout = 2*60;

int main(int argc, char **argv)
{
	struct gbs *gbs;
	int subsong = -1;

	if (argc < 2) {
		printf("Usage: %s <gbs-file> [<subsong>]\n", argv[0]);
		exit(1);
	}

	precalc_notes();
	precalc_vols();

	open_dsp();
	gbhw_setcallback(callback, NULL);
	gbhw_setrate(44100);

	if (argc == 3) sscanf(argv[2], "%d", &subsong);
	gbs = gbs_open(argv[1]);
	if (gbs == NULL) exit(1);
	gbs_printinfo(gbs, 0);
	puts("");
	if (subsong == -1) subsong = gbs->defaultsong;
	gbs_playsong(gbs, subsong);
	printf("\n\n");
	while (1) {
		static long long clock = 0;
		static int silencectr = 0;
		int cycles = gbhw_step();
		statuscnt -= cycles;
		clock += cycles;
		if (statuscnt < 0) {
			int time = clock / 4194304;
			int timem = time / 60;
			int times = time % 60;
			int ni1 = getnote(gbhw_ch[0].div_tc);
			int ni2 = getnote(gbhw_ch[1].div_tc);
			int ni3 = getnote(gbhw_ch[2].div_tc);
			char *n1 = &notelookup[4*ni1];
			char *n2 = &notelookup[4*ni2];
			char *n3 = &notelookup[4*ni3];
			char *v1 = &vollookup[5* (gbhw_ch[0].volume & 15)];
			char *v2 = &vollookup[5* (gbhw_ch[1].volume & 15)];
			char *v3 = &vollookup[5* (((3-((gbhw_ch[2].volume+3)&3)) << 2) & 15)];
			char *v4 = &vollookup[5* (gbhw_ch[3].volume & 15)];
			char *songtitle;
			int len = gbs->subsong_info[subsong - 1].len / 128;
			int lenm, lens;

			if (len == 0) len = timeout;
			lenm = len / 60;
			lens = len % 60;

			statuscnt += statustc;

			if (!gbhw_ch[0].volume) n1 = "---";
			if (!gbhw_ch[1].volume) n2 = "---";
			if (!gbhw_ch[2].volume) n3 = "---";

			songtitle = gbs->subsong_info[subsong - 1].title;
			if (!songtitle) songtitle="Untitled";
			printf("\r\033[A\033[A"
			       "Song %3d/%3d (%s)\033[K\n"
			       "%02d:%02d/%02d:%02d  ch1: %s %s  ch2: %s %s  ch3: %s %s  ch4: %s\n",
			       subsong, gbs->songs, songtitle,
			       timem, times, lenm, lens,
			       n1, v1, n2, v2, n3, v3, v4);
			fflush(stdout);

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

			if (time >= timeout || silencectr > 100) {
				subsong++;
				silencectr = 0;
				clock = 0;
				gbs_playsong(gbs, subsong);
			}
		}
	}
	return 0;
}
