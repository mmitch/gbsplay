/* $Id: gbsplay.c,v 1.39 2003/08/31 17:46:34 ranma Exp $
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

#include "config.h"

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

static int dspfd;

void callback(void *buf, int len, void *priv)
{
	write(dspfd, buf, len);
}

static int rate = 44100;
static int usestdout = 0;
static int quiet = 0;

void open_dsp(void)
{
	int c;
	int flags;

	if (usestdout) {
		dspfd = STDOUT_FILENO;
		return;
	}
	if ((dspfd = open("/dev/dsp", O_WRONLY|O_NONBLOCK)) == -1) {
		fprintf(stderr, "Could not open /dev/dsp: %s\n", strerror(errno));
		exit(1);
	}
	if ((flags = fcntl(dspfd, F_GETFL)) == -1) {
		fprintf(stderr, "fcntl failed: %s\n", strerror(errno));
	} else {
		fcntl(dspfd, F_SETFL, flags & ~O_NONBLOCK);
	}

	c=AFMT_S16_LE;
	if ((ioctl(dspfd, SNDCTL_DSP_SETFMT, &c)) == -1) {
		fprintf(stderr, "ioctl(dspfd, SNDCTL_DSP_SETFMT, %d) failed: %s\n", c, strerror(errno));
		exit(1);
	}
	c = rate;
	if ((ioctl(dspfd, SNDCTL_DSP_SPEED, &c)) == -1) {
		fprintf(stderr, "ioctl(dspfd, SNDCTL_DSP_SPEED, %d) failed: %s\n", rate, strerror(errno));
		exit(1);
	}
	if (c != rate) {
		fprintf(stderr, "Requested rate %dHz, got %dHz.\n", rate, c);
		rate = c;
	}
	c=1;
	if ((ioctl(dspfd, SNDCTL_DSP_STEREO, &c)) == -1) {
		fprintf(stderr, "ioctl(dspfd, SNDCTL_DSP_STEREO, %d) failed: %s\n", c, strerror(errno));
		exit(1);
	}
	c=(4 << 16) + 11;
	if ((ioctl(dspfd, SNDCTL_DSP_SETFRAGMENT, &c)) == -1)
		fprintf(stderr, "ioctl(dspfd, SNDCTL_DSP_SETFRAGMENT, %08x) failed: %s\n", c, strerror(errno));
}

static char *myname;
static int subsongtimeout = 2*60;
static int silencetimeout = 2;

void usage(int exitcode)
{
	FILE *out = exitcode ? stderr : stdout;
	fprintf(out,
	        "Usage: %s [option(s)] <gbs-file> [subsong number]\n"
	        "\n"
	        "Available options are:\n"
	        "  -h	display this help and exit\n"
	        "  -r	set samplerate (%dHz)\n"
	        "  -s	write to stdout\n"
	        "  -t	set subsong timeout (%d seconds)\n"
	        "  -T	set silence timeout (%d seconds)\n"
	        "  -V	print version and exit\n",
	        myname,
	        rate,
	        subsongtimeout,
	        silencetimeout);
	exit(exitcode);
}

void version(void)
{
	puts("gbsplay " GBS_VERSION);
	exit(0);
}

void parseopts(int *argc, char ***argv)
{
	int res;
	myname = *argv[0];
	while ((res = getopt(*argc, *argv, "hqr:st:T:V")) != -1) {
		switch (res) {
		default:
			usage(1);
			break;
		case 'h':
			usage(0);
			break;
		case 'q':
			quiet = 1;
			break;
		case 'r':
			sscanf(optarg, "%d", &rate);
			break;
		case 's':
			usestdout = 1;
			quiet = 1;
			break;
		case 't':
			sscanf(optarg, "%d", &subsongtimeout);
			break;
		case 'T':
			sscanf(optarg, "%d", &silencetimeout);
			break;
		case 'V':
			version();
			break;
		}
	}
	*argc -= optind;
	*argv += optind;
}

static int quit = 0;
static int silencectr = 0;
static long long clock = 0;
static int subsong = -1;

static void handletimeouts(struct gbs *gbs)
{
	int time = clock / 4194304;

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

	if ((subsongtimeout && time >= subsongtimeout) ||
	    (silencetimeout && silencectr > 50*silencetimeout)) {
		subsong++;
		if (subsong >= gbs->songs) quit = 1;
		silencectr = 0;
		clock = 0;
		gbs_playsong(gbs, subsong);
	}
}

static void handleuserinput(struct gbs *gbs)
{
	char c;

	if (read(STDIN_FILENO, &c, 1) != -1){
		switch (c) {
		case 'p':
		case 'n':
			subsong += c == 'n' ? 1 : -1;
			subsong %= gbs->songs;
			silencectr = 0;
			clock = 0;
			gbs_playsong(gbs, subsong);
			break;
		case 'q':
		case 27:
			quit = 1;
			break;
		}
	}
}

static void printstatus(struct gbs *gbs)
{
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
	int len = gbs->subsong_info[subsong].len / 128;
	int lenm, lens;

	if (len == 0) len = subsongtimeout;
	lenm = len / 60;
	lens = len % 60;

	if (!gbhw_ch[0].volume) n1 = "---";
	if (!gbhw_ch[1].volume) n2 = "---";
	if (!gbhw_ch[2].volume) n3 = "---";

	songtitle = gbs->subsong_info[subsong].title;
	if (!songtitle) songtitle="Untitled";
	printf("\r\033[A\033[A"
	       "Song %3d/%3d (%s)\033[K\n"
	       "%02d:%02d/%02d:%02d  ch1: %s %s  ch2: %s %s  ch3: %s %s  ch4: %s\n",
	       subsong+1, gbs->songs, songtitle,
	       timem, times, lenm, lens,
	       n1, v1, n2, v2, n3, v3, v4);
	fflush(stdout);
}

static int statustc = 83886;
static int statuscnt;

int main(int argc, char **argv)
{
	struct gbs *gbs;

	parseopts(&argc, &argv);

	if (argc < 1) usage(1);

	precalc_notes();
	precalc_vols();

	open_dsp();
	gbhw_setcallback(callback, NULL);
	gbhw_setrate(rate);

	if (argc == 2) {
		sscanf(argv[1], "%d", &subsong);
		subsong--;
	}
	gbs = gbs_open(argv[0]);
	if (gbs == NULL) exit(1);
	if (subsong == -1) subsong = gbs->defaultsong - 1;
	gbs_playsong(gbs, subsong);
	if (!quiet) {
		gbs_printinfo(gbs, 0);
		printf("\n\n\n");
	}
	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
	while (!quit) {
		int cycles = gbhw_step();

		if (cycles < 0) quit = 1;

		statuscnt -= cycles;
		clock += cycles;

		if (statuscnt < 0) {
			statuscnt += statustc;
			if (!quiet) printstatus(gbs);
			handleuserinput(gbs);
			handletimeouts(gbs);
		}
	}
	return 0;
}
