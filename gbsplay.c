/* $Id: gbsplay.c,v 1.62 2003/10/27 21:52:29 ranma Exp $
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
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <termios.h>
#include <signal.h>
#include <time.h>

#include "gbhw.h"
#include "gbcpu.h"
#include "gbs.h"
#include "cfgparser.h"
#include "util.h"

#include "config.h"

#define LN2 .69314718055994530941
#define MAGIC 5.78135971352465960412
#define FREQ(x) (262144 / x)
/* #define NOTE(x) ((log(FREQ(x))/LN2 - log(55)/LN2)*12 + .2) */
#define NOTE(x) ((int)((log(FREQ(x))/LN2 - MAGIC)*12 + .2))

#define MAXOCTAVE 9

/* player modes */
#define PLAYMODE_LINEAR  1
#define PLAYMODE_SHUFFLE 2
#define PLAYMODE_RANDOM  3

/* lookup tables */
static char notelookup[4*MAXOCTAVE*12];
static char vollookup[5*16];
static const char vols[5] = " -=#%";

/* global variables */
static char *myname;
static int dspfd;
static int quit = 0;
static int silencectr = 0;
static long long ticks = 0;
static struct termios ots;
static int *subsong_playlist;
static int subsong_playlist_idx = 0;
static int pause_mode = 0;

#define DEFAULT_REFRESH_DELAY 33

static int refresh_delay = DEFAULT_REFRESH_DELAY; /* msec */
static int refresh_freq = 1000 / DEFAULT_REFRESH_DELAY;

/* default values */
static int playmode = PLAYMODE_LINEAR;
static int quiet = 0;
static int rate = 44100;
static int silence_timeout = 2;
static int fadeout = 3;
static int subsong_gap = 2;
static int subsong = -1;
static int subsong_stop = -1;
static int subsong_timeout = 2*60;
static int usestdout = 0;

static char *cfgfile = ".gbsplayrc";


static int getnote(int div)
{
	int n = 0;

	if (div>0) {
		n = NOTE(div);
	}

	if (n < 0) {
		n = 0;
	} else if (n >= MAXOCTAVE*12) {
		n = MAXOCTAVE-1;
	}

	return n;
}

static void precalc_notes(void)
{
	int i;
	for (i=0; i<MAXOCTAVE*12; i++) {
		char *s = notelookup + 4*i;
		int n = i % 12;

		s[2] = '0' + i / 12;
		n += (n > 2) + (n > 7);
		s[0] = 'A' + (n >> 1);
		if (n & 1) {
			s[1] = '#';
		} else {
			s[1] = '-';
		}
	}
}

static char *reverse_vol(char *s)
{
	static char buf[5];
	int i;

	for (i=0; i<4; i++) {
		buf[i] = s[3-i];
	}
	buf[4] = 0;

	return buf;
}

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

void callback(void *buf, int len, void *priv)
{
	write(dspfd, buf, len);
}

static struct cfg_option options[] = {
	{ "rate", &rate, cfg_int },
	{ "quiet", &quiet, cfg_int },
	{ "subsong_timeout", &subsong_timeout, cfg_int },
	{ "subsong_gap", &subsong_gap, cfg_int },
	{ "fadeout", &fadeout, cfg_int },
	{ "silence_timeout", &silence_timeout, cfg_int },
	/* playmode not implemented yet */
	{ NULL, NULL, NULL }
};

static int *setup_playlist(int songs)
/* setup a playlist in shuffle mode */
{
	int i;
	int *playlist;
	
	playlist = (int*) calloc( songs, sizeof(int) );
	for (i=0; i<songs; i++) {
		playlist[i] = i;
	}

	shuffle_int(playlist, songs);

	return playlist;
}

static int get_next_subsong(struct gbs *gbs)
/* returns the number of the subsong that is to be played next */
{
	int next = -1;
	switch (playmode) {

	case PLAYMODE_SHUFFLE:
		next = rand_int(gbs->songs);
		break;

	case PLAYMODE_RANDOM:
		subsong_playlist_idx++;
		if (subsong_playlist_idx == gbs->songs) {
			free(subsong_playlist);
			subsong_playlist = setup_playlist(gbs->songs);
			subsong_playlist_idx = 0;
		}
		next = subsong_playlist[subsong_playlist_idx];
		break;

	case PLAYMODE_LINEAR:
		next = subsong + 1;
		break;
	}
	return next;
}

static void setup_playmode(struct gbs *gbs)
/* initializes the chosen playmode (set start subsong etc.) */
{
	int temp;

	switch (playmode) {

	case PLAYMODE_SHUFFLE:
		if (subsong == -1) {
			subsong = get_next_subsong(gbs);
		}

	case PLAYMODE_RANDOM:
		subsong_playlist = setup_playlist(gbs->songs);
		subsong_playlist_idx = 0;
		if (subsong == -1) {
			subsong = subsong_playlist[0];
		} else {
			/* rotate playlist until desired start song is first */
			while (subsong_playlist[0] != subsong) {
				temp = subsong_playlist[gbs->songs-1];
				memmove(subsong_playlist+1, subsong_playlist, (gbs->songs - 1) * sizeof(int));
				subsong_playlist[0] = temp;
			}
		}

	case PLAYMODE_LINEAR:
		if (subsong == -1) {
			subsong = gbs->defaultsong - 1;
		}
		break;
	}
}

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
		fprintf(stderr, "fcntl(F_GETFL) failed: %s\n", strerror(errno));
	} else if (fcntl(dspfd, F_SETFL, flags & ~O_NONBLOCK) == -1) {
		fprintf(stderr, "fcntl(F_SETFL, flags&~O_NONBLOCK) failed: %s\n", strerror(errno));
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

void usage(int exitcode)
{
	FILE *out = exitcode ? stderr : stdout;
	fprintf(out,
	        "Usage: %s [option(s)] <gbs-file> [start_at_subsong [stop_at_subsong] ]\n"
	        "\n"
	        "Available options are:\n"
	        "  -f  set fadeout (%d seconds)\n"
	        "  -g  set subsong gap (%d seconds)\n"
	        "  -h  display this help and exit\n"
	        "  -h  display this help and exit\n"
	        "  -q  quiet\n"
	        "  -r  set samplerate (%dHz)\n"
	        "  -s  write to stdout\n"
	        "  -t  set subsong timeout (%d seconds)\n"
	        "  -T  set silence timeout (%d seconds)\n"
	        "  -V  print version and exit\n"
		"  -z  play subsongs in random mode\n"
		"  -Z  play subsongs in shuffle mode (repetitions possible)\n",
	        myname,
	        rate,
	        subsong_timeout,
	        silence_timeout,
		fadeout,
		subsong_gap);
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
	while ((res = getopt(*argc, *argv, "hqr:st:T:f:g:VzZ")) != -1) {
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
			sscanf(optarg, "%d", &subsong_timeout);
			break;
		case 'T':
			sscanf(optarg, "%d", &silence_timeout);
			break;
		case 'f':
			sscanf(optarg, "%d", &fadeout);
			break;
		case 'g':
			sscanf(optarg, "%d", &subsong_gap);
			break;
		case 'V':
			version();
			break;
		case 'z':
		  playmode = PLAYMODE_RANDOM;
		  break;
		case 'Z':
		  playmode = PLAYMODE_SHUFFLE;
		  break;
		}
	}
	*argc -= optind;
	*argv += optind;
}

static int lmin, lmax, rmin, rmax, lvol, rvol;

static void handletimeouts(struct gbs *gbs)
{
	int time = ticks / 4194304;
	int next_subsong;

	if (lmin == lmax && rmin == rmax) {
		silencectr++;
	} else silencectr = 0;

	if (fadeout && subsong_timeout && time >= subsong_timeout)
		gbhw_master_fade(128/fadeout, 0);
	if (subsong_timeout && time >= (subsong_timeout+fadeout))
		gbhw_master_fade(128*16, 0);

	if ((subsong_timeout && time >= (subsong_timeout+fadeout+subsong_gap)) ||
	    (silence_timeout && silencectr > refresh_freq*silence_timeout)) {
		if (subsong == subsong_stop) {
			quit = 1;
		}
		next_subsong = get_next_subsong(gbs);
		if (next_subsong >= gbs->songs) {
			quit = 1;
		} else {
			subsong = next_subsong;
		}
		silencectr = 0;
		ticks = 0;
		gbs_playsong(gbs, subsong);
	}
}

static void handleuserinput(struct gbs *gbs)
{
	char c;

	if (read(STDIN_FILENO, &c, 1) != -1) {
		switch (c) {
		case 'p':
		case 'n':
			subsong += c == 'n' ? 1 : gbs->songs-1;
			subsong %= gbs->songs;
			silencectr = 0;
			ticks = 0;
			gbs_playsong(gbs, subsong);
			break;
		case 'q':
		case 27:
			quit = 1;
			break;
		case ' ':
			pause_mode = !pause_mode;
			gbhw_pause(pause_mode);
			break;
		case '1':
		case '2':
		case '3':
		case '4':
			gbhw_ch[c-'1'].mute ^= 1;
			break;
		}
	}
}

static char *notestring(int ch)
{
	int n;

	if (gbhw_ch[ch].mute) return "-M-";

	if (gbhw_ch[ch].volume == 0 ||
	    gbhw_ch[ch].master == 0) return "---";

	n = getnote(gbhw_ch[ch].div_tc);
	if (ch != 3) return &notelookup[4*n];
	else return "nse";
}

static int chvol(int ch)
{
	int v;

	if (gbhw_ch[ch].mute ||
	    gbhw_ch[ch].master == 0) return 0;

	if (ch == 2)
		v = (3-((gbhw_ch[2].volume+3)&3)) << 2;
	else v = gbhw_ch[ch].volume;

	return v;
}

static char *volstring(int v)
{
	if (v < 0) v = 0;
	if (v > 15) v = 15;

	return &vollookup[5*v];
}

static void printstatus(struct gbs *gbs)
{
	int time = ticks / 4194304;
	int timem = time / 60;
	int times = time % 60;
	char *songtitle;
	int len = gbs->subsong_info[subsong].len / 1024;
	int lenm, lens;

	if (len == 0) {
		len = subsong_timeout;
	}
	lenm = len / 60;
	lens = len % 60;

	songtitle = gbs->subsong_info[subsong].title;
	if (!songtitle) {
		songtitle="Untitled";
	}
	printf("\r\033[A\033[A"
	       "Song %3d/%3d (%s)\033[K\n"
	       "%02d:%02d/%02d:%02d  %s %s  %s %s  %s %s  %s %s  [%s|%s]\n",
	       subsong+1, gbs->songs, songtitle,
	       timem, times, lenm, lens,
	       notestring(0), volstring(chvol(0)),
	       notestring(1), volstring(chvol(1)),
	       notestring(2), volstring(chvol(2)),
	       notestring(3), volstring(chvol(3)),
	       reverse_vol(volstring(lvol/1024)),
	       volstring(rvol/1024));
	fflush(stdout);
}

void exit_handler(int signum)
{
	printf("\nCatched signal %d, exiting...\n", signum);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &ots);
	exit(1);
}

void stop_handler(int signum)
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &ots);
}

void cont_handler(int signum)
{
	struct termios ts;

	tcgetattr(STDIN_FILENO, &ts);
	ots = ts;
	ts.c_lflag &= ~(ICANON | ECHO | ECHONL);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &ts);
	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

int main(int argc, char **argv)
{
	struct gbs *gbs;
	char *homedir = getenv("HOME");
	char *usercfg = malloc(strlen(homedir) + strlen(cfgfile) + 2);
	struct termios ts;
	struct sigaction sa;

	srand(time(0)+getpid());  /* initialize RNG */

	sprintf(usercfg, "%s/%s", homedir, cfgfile);
	cfg_parse("/etc/gbsplayrc", options);
	cfg_parse(usercfg, options);
	parseopts(&argc, &argv);

	if (argc < 1) {
		usage(1);
	}

	precalc_notes();
	precalc_vols();

	open_dsp();
	gbhw_setcallback(callback, NULL);
	gbhw_setrate(rate);

	if (argc >= 2) {
		sscanf(argv[1], "%d", &subsong);
		subsong--;
	}

	if (argc >= 3) {
		sscanf(argv[2], "%d", &subsong_stop);
		subsong_stop--;
	}

	gbs = gbs_open(argv[0]);
	if (gbs == NULL) {
		exit(1);
	}

	/* sanitize commandline values */
	if (subsong <  0) {
		subsong = 0;
	} else if (subsong >= gbs->songs) {
		subsong = gbs->songs-1;
	}
	if (subsong_stop <  0) {
		subsong_stop = -1;
	} else if (subsong_stop >= gbs->songs) {
		subsong_stop = -1;
	}
	
	setup_playmode(gbs);
	gbs_playsong(gbs, subsong);
	if (!quiet) {
		gbs_printinfo(gbs, 0);
		puts("\ncommands:  [p]revious subsong   [n]ext subsong   [q]uit player");
		puts(  "           [ ] pause/resume   [1-4] mute channel");
		puts("\n\n"); /* additional newlines for the status display */
	}
	tcgetattr(STDIN_FILENO, &ts);
	ots = ts;
	ts.c_lflag &= ~(ICANON | ECHO | ECHONL);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &ts);

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = exit_handler;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGSEGV, &sa, NULL);
	sa.sa_handler = stop_handler;
	sigaction(SIGSTOP, &sa, NULL);
	sa.sa_handler = cont_handler;
	sigaction(SIGCONT, &sa, NULL);

	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
	while (!quit) {
		int cycles = gbhw_step(refresh_delay);

		if (cycles < 0) {
			quit = 1;
		}

		ticks += cycles;

		gbhw_getminmax(&lmin, &lmax, &rmin, &rmax);
		lvol = (-lmin > lmax) ? -lmin : lmax;
		rvol = (-rmin > rmax) ? -rmin : rmax;
		if (!quiet) printstatus(gbs);
		handletimeouts(gbs);
		handleuserinput(gbs);
	}
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &ots);
	return 0;
}
