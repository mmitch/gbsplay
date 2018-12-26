/*
 * xgbsplay is an X11 frontend for gbsplay, the Gameboy sound player
 *
 * 2003-2005,2008,2018 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                            Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL.
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <termios.h>
#include <signal.h>
#include <time.h>

#include <X11/Xlib.h>

#include "gbhw.h"
#include "gbcpu.h"
#include "gbs.h"
#include "cfgparser.h"
#include "util.h"
#include "plugout.h"

#define LN2 .69314718055994530941
#define MAGIC 5.78135971352465960412
#define FREQ(x) (262144 / x)
/* #define NOTE(x) ((log(FREQ(x))/LN2 - log(55)/LN2)*12 + .2) */
#define NOTE(x) ((long)((log(FREQ(x))/LN2 - MAGIC)*12 + .2))

#define MAXOCTAVE 9

/* player modes */
#define PLAYMODE_LINEAR  1
#define PLAYMODE_RANDOM  2
#define PLAYMODE_SHUFFLE 3

/* lookup tables */
static char notelookup[4*MAXOCTAVE*12];
static char vollookup[5*16];
static const char vols[5] = " -=#%";

/* global variables */
static char *myname;
static long quit = 0;
static struct termios ots;
static long *subsong_playlist;
static long subsong_playlist_idx = 0;
static long pause_mode = 0;

unsigned long random_seed;

#define DEFAULT_REFRESH_DELAY 33

static long refresh_delay = DEFAULT_REFRESH_DELAY; /* msec */

#define STATUSTEXT_LENGTH 256
static char statustext[STATUSTEXT_LENGTH];
static char oldstatustext[STATUSTEXT_LENGTH];

static Display *display;
static Window window;
static int screen;
static GC gc;

/* default values */
static long playmode = PLAYMODE_LINEAR;
static long loopmode = 0;
static enum plugout_endian endian = PLUGOUT_ENDIAN_NATIVE;
static long rate = 44100;
static long silence_timeout = 2;
static long fadeout = 3;
static long subsong_gap = 2;
static long subsong_start = -1;
static long subsong_stop = -1;
static long subsong_timeout = 2*60;

static const char cfgfile[] = ".gbsplayrc";

static char *sound_name = PLUGOUT_DEFAULT;
static char *filter_type = GBHW_CFG_FILTER_DMG;
static char *sound_description;
static plugout_open_fn  sound_open;
static plugout_skip_fn  sound_skip;
static plugout_pause_fn sound_pause;
static plugout_io_fn    sound_io;
static plugout_write_fn sound_write;
static plugout_close_fn sound_close;

static int16_t samples[4096];
static struct gbhw_buffer buf = {
.data = samples,
.pos  = 0,
.bytes = sizeof(samples),
};

/* configuration directives */
static const struct cfg_option options[] = {
	{ "endian", &endian, cfg_endian },
	{ "fadeout", &fadeout, cfg_long },
	{ "filter_type", &filter_type, cfg_string },
	{ "loop", &loopmode, cfg_long },
	{ "output_plugin", &sound_name, cfg_string },
	{ "rate", &rate, cfg_long },
	{ "refresh_delay", &refresh_delay, cfg_long },
	{ "silence_timeout", &silence_timeout, cfg_long },
	{ "subsong_gap", &subsong_gap, cfg_long },
	{ "subsong_timeout", &subsong_timeout, cfg_long },
	/* playmode not implemented yet */
	{ NULL, NULL, NULL }
};

static regparm void precalc_notes(void)
{
	long i;
	for (i=0; i<MAXOCTAVE*12; i++) {
		char *s = notelookup + 4*i;
		long n = i % 12;

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

static regparm void precalc_vols(void)
{
	long i, k;
	for (k=0; k<16; k++) {
		long j;
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

static regparm void swap_endian(struct gbhw_buffer *buf)
{
	long i;

	for (i=0; i<buf->bytes/sizeof(short); i++) {
		short x = buf->data[i];
		buf->data[i] = ((x & 0xff) << 8) | (x >> 8);
	}
}

static regparm void iocallback(long cycles, uint32_t addr, uint8_t val, void *priv)
{
	sound_io(cycles, addr, val);
}

static regparm void callback(struct gbhw_buffer *buf, void *priv)
{
	if ((is_le_machine() && endian == PLUGOUT_ENDIAN_BIG) ||
	    (is_be_machine() && endian == PLUGOUT_ENDIAN_LITTLE)) {
		swap_endian(buf);
	}
	sound_write(buf->data, buf->pos*2*sizeof(int16_t));
	buf->pos = 0;
}

static regparm long *setup_playlist(long songs)
/* setup a playlist in shuffle mode */
{
	long i;
	long *playlist;
	
	playlist = (long*) calloc( songs, sizeof(long) );
	for (i=0; i<songs; i++) {
		playlist[i] = i;
	}

	/* reinit RNG with current seed - playlists shall be reproducible! */
	srand(random_seed);
	shuffle_long(playlist, songs);

	return playlist;
}

static regparm long get_next_subsong(struct gbs *gbs)
/* returns the number of the subsong that is to be played next */
{
	long next = -1;
	switch (playmode) {

	case PLAYMODE_RANDOM:
		next = rand_long(gbs->songs);
		break;

	case PLAYMODE_SHUFFLE:
		subsong_playlist_idx++;
		if (subsong_playlist_idx == gbs->songs) {
			free(subsong_playlist);
			random_seed++;
			subsong_playlist = setup_playlist(gbs->songs);
			subsong_playlist_idx = 0;
		}
		next = subsong_playlist[subsong_playlist_idx];
		break;

	case PLAYMODE_LINEAR:
		next = gbs->subsong + 1;
		break;
	}
	return next;
}

static regparm int get_prev_subsong(struct gbs *gbs)
/* returns the number of the subsong that has been played previously */
{
	int prev = -1;
	switch (playmode) {

	case PLAYMODE_RANDOM:
		prev = rand_long(gbs->songs);
		break;

	case PLAYMODE_SHUFFLE:
		subsong_playlist_idx--;
		if (subsong_playlist_idx == -1) {
			free(subsong_playlist);
			random_seed--;
			subsong_playlist = setup_playlist(gbs->songs);
			subsong_playlist_idx = gbs->songs-1;
		}
		prev = subsong_playlist[subsong_playlist_idx];
		break;

	case PLAYMODE_LINEAR:
		prev = gbs->subsong - 1;
		break;
	}
	return prev;
}

static regparm void setup_playmode(struct gbs *gbs)
/* initializes the chosen playmode (set start subsong etc.) */
{
	switch (playmode) {

	case PLAYMODE_RANDOM:
		if (gbs->subsong == -1) {
			gbs->subsong = get_next_subsong(gbs);
		}
		break;

	case PLAYMODE_SHUFFLE:
		subsong_playlist = setup_playlist(gbs->songs);
		subsong_playlist_idx = 0;
		if (gbs->subsong == -1) {
			gbs->subsong = subsong_playlist[0];
		} else {
			/* randomize playlist until desired start song is first */
			/* (rotation does not work because this must be reproducible */
			/* by setting random_seed to the old value */
			while (subsong_playlist[0] != gbs->subsong) {
				random_seed++;
				subsong_playlist = setup_playlist(gbs->songs);
			}
		}
		break;

	case PLAYMODE_LINEAR:
		if (gbs->subsong == -1) {
			gbs->subsong = gbs->defaultsong - 1;
		}
		break;
	}
}

static regparm long nextsubsong_cb(struct gbs *gbs, void *priv)
{
	long subsong = get_next_subsong(gbs);

	if (gbs->subsong == subsong_stop ||
	    subsong >= gbs->songs) {
		if (loopmode) {
			subsong = subsong_start;
			setup_playmode(gbs);
		} else {
			return false;
		}
	}

	gbs_init(gbs, subsong);
	if (sound_skip)
		sound_skip(subsong);
	return true;
}

char *endian_str(long endian)
{
	switch (endian) {
	case PLUGOUT_ENDIAN_BIG: return "big";
	case PLUGOUT_ENDIAN_LITTLE: return "little";
	case PLUGOUT_ENDIAN_NATIVE: return "native";
	default: return "invalid";
	}
}

static regparm void usage(long exitcode)
{
	FILE *out = exitcode ? stderr : stdout;
	fprintf(out,
		_("Usage: %s [option(s)] <gbs-file> [start_at_subsong [stop_at_subsong] ]\n"
		  "\n"
		  "Available options are:\n"
		  "  -E        endian, b == big, l == little, n == native (%s)\n"
		  "  -f        set fadeout (%ld seconds)\n"
		  "  -g        set subsong gap (%ld seconds)\n"
		  "  -h        display this help and exit\n"
		  "  -H        set output high-pass type (%s)\n"
		  "  -l        loop mode\n"
		  "  -o        select output plugin (%s)\n"
		  "            'list' shows available plugins\n"
		  "  -r        set samplerate (%ldHz)\n"
		  "  -R        set refresh delay (%ld milliseconds)\n"
		  "  -t        set subsong timeout (%ld seconds)\n"
		  "  -T        set silence timeout (%ld seconds)\n"
		  "  -V        print version and exit\n"
		  "  -z        play subsongs in shuffle mode\n"
		  "  -Z        play subsongs in random mode (repetitions possible)\n"
		  "  -1 to -4  mute a channel on startup\n"),
		myname,
		endian_str(endian),
		fadeout,
		subsong_gap,
		_(filter_type),
		sound_name,
		rate,
		refresh_delay,
		subsong_timeout,
		silence_timeout);
	exit(exitcode);
}

static regparm void version(void)
{
	puts("gbsplay " GBS_VERSION);
	exit(0);
}

static regparm void parseopts(int *argc, char ***argv)
{
	long res;
	myname = *argv[0];
	while ((res = getopt(*argc, *argv, "1234c:E:f:g:hH:lo:r:R:t:T:VzZ")) != -1) {
		switch (res) {
		default:
			usage(1);
			break;
		case '1':
		case '2':
		case '3':
		case '4':
			gbhw_ch[res-'1'].mute ^= 1;
			break;
		case 'c':
			cfg_parse(optarg, options);
			break;
		case 'E':
			if (strcasecmp(optarg, "b") == 0) {
				endian = PLUGOUT_ENDIAN_BIG;
			} else if (strcasecmp(optarg, "l") == 0) {
				endian = PLUGOUT_ENDIAN_LITTLE;
			} else if (strcasecmp(optarg, "n") == 0) {
				endian = PLUGOUT_ENDIAN_NATIVE;
			} else {
				printf(_("\"%s\" is not a valid endian.\n\n"), optarg);
				usage(1);
			}
			break;
		case 'f':
			sscanf(optarg, "%ld", &fadeout);
			break;
		case 'g':
			sscanf(optarg, "%ld", &subsong_gap);
			break;
		case 'h':
			usage(0);
			break;
		case 'H':
			filter_type = optarg;
			break;
		case 'l':
			loopmode = 1;
			break;
		case 'o':
			sound_name = optarg;
			break;
		case 'r':
			sscanf(optarg, "%ld", &rate);
			break;
		case 'R':
			sscanf(optarg, "%ld", &refresh_delay);
			break;
		case 't':
			sscanf(optarg, "%ld", &subsong_timeout);
			break;
		case 'T':
			sscanf(optarg, "%ld", &silence_timeout);
			break;
		case 'V':
			version();
			break;
		case 'z':
			playmode = PLAYMODE_SHUFFLE;
			break;
		case 'Z':
			playmode = PLAYMODE_RANDOM;
			break;
		}
	}
	*argc -= optind;
	*argv += optind;
}

static regparm void handleuserinput(struct gbs *gbs)
{
	char c;

	if (read(STDIN_FILENO, &c, 1) != -1) {
		switch (c) {
		case 'p':
			gbs->subsong = get_prev_subsong(gbs);
			while (gbs->subsong < 0) {
				gbs->subsong += gbs->songs;
			}
			gbs_init(gbs, gbs->subsong);
			if (sound_skip)
				sound_skip(gbs->subsong);
			break;
		case 'n':
			gbs->subsong = get_next_subsong(gbs);
			gbs->subsong %= gbs->songs;
			gbs_init(gbs, gbs->subsong);
			if (sound_skip)
				sound_skip(gbs->subsong);
			break;
		case 'q':
		case 27:
			quit = 1;
			break;
		case ' ':
			pause_mode = !pause_mode;
			gbhw_pause(pause_mode);
			if (sound_pause) sound_pause(pause_mode);
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

static regparm void updatetitle(struct gbs *gbs)
{
	long time = gbs->ticks / GBHW_CLOCK;
	long timem = time / 60;
	long times = time % 60;
	char *songtitle;
	long len = gbs->subsong_info[gbs->subsong].len / 1024;
	long lenm, lens;

	if (len == 0) {
		len = subsong_timeout;
	}
	lenm = len / 60;
	lens = len % 60;

	songtitle = gbs->subsong_info[gbs->subsong].title;
	if (!songtitle) {
		songtitle=_("Untitled");
	}
	snprintf(statustext, STATUSTEXT_LENGTH, /* or use sizeof(statustext) ?? */
		 "xgbsplay"
		 " %02ld:%02ld/%02ld:%02ld"
		 " Song %3d/%3d (%s)",
		 timem, times, lenm, lens,
		 gbs->subsong+1, gbs->songs, songtitle);

	if (strncmp(statustext, oldstatustext, STATUSTEXT_LENGTH) != 0) {  /* or use sizeof(statustext) ?? or strcmp() ?? */
		strncpy(oldstatustext, statustext, STATUSTEXT_LENGTH);
		XStoreName(display, window, statustext);
	}
}

/*
 * signal handlers and main may not use regparm
 * unless libc is using regparm too...
 */
void exit_handler(int signum)
{
	printf(_("\nCaught signal %d, exiting...\n"), signum);
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

static regparm void select_plugin(void)
{
	const struct output_plugin *plugout;

	if (strcmp(sound_name, "list") == 0) {
		plugout_list_plugins();
		exit(0);
	}

	plugout = plugout_select_by_name(sound_name);
	if (plugout == NULL) {
		fprintf(stderr, _("\"%s\" is not a known output plugin.\n\n"),
		        sound_name);
		exit(1);
	}

	sound_open = plugout->open;
	sound_skip = plugout->skip;
	sound_io = plugout->io;
	sound_write = plugout->write;
	sound_close = plugout->close;
	sound_pause = plugout->pause;
	sound_description = plugout->description;
}

#define GRID(s,t,i) ((t * i / s))

static void drawbuttons()
{
	XWindowAttributes attrs;
	XSegment line[7];
	XPoint triangle[3];
	int i;

	XGetWindowAttributes(display, window, &attrs);

	/* erase */
	XFillRectangle(display, window, DefaultGC(display, screen),
		       0, 0, attrs.width, attrs.height);

	/* button boxes */
	line[0].x1 = 0;
	line[0].y1 = 0;
	line[0].x2 = line[0].x2;
	line[0].y2 = attrs.height - 1;

	for (i = 1; i<5; i++) {
		line[i].x1 = attrs.width * i / 4 - 1;
		line[i].y1 = 0;
		line[i].x2 = line[i].x1;
		line[i].y2 = attrs.height - 1;
	}

	line[5].x1 = 0;
	line[5].y1 = 0;
	line[5].x2 = attrs.width - 1;
	line[5].y2 = line[5].y1;

	line[6].x1 = 0;
	line[6].y1 = attrs.height - 1;
	line[6].x2 = attrs.width - 1;
	line[6].y2 = line[6].y1;

	XDrawSegments(display, window, gc, line, 7);

	/* prev */
	triangle[0].x = GRID(20, 4, attrs.width); triangle[0].y = GRID(5, 1, attrs.height);
	triangle[1].x = GRID(20, 4, attrs.width); triangle[1].y = GRID(5, 4, attrs.height);
	triangle[2].x = GRID(20, 1, attrs.width); triangle[2].y = GRID(2, 1, attrs.height);
	XFillPolygon(display, window, gc, triangle, 3, Convex, CoordModeOrigin);

	/* pause */
	XFillRectangle(display, window, gc,
		       GRID(20, 6, attrs.width), GRID(5, 1, attrs.height),
		       GRID(20, 1, attrs.width), GRID(5, 3, attrs.height));
	XFillRectangle(display, window, gc,
		       GRID(20, 8, attrs.width), GRID(5, 1, attrs.height),
		       GRID(20, 1, attrs.width), GRID(5, 3, attrs.height));

	/* stop */
	XFillRectangle(display, window, gc,
		       GRID(20, 11, attrs.width), GRID(5, 1, attrs.height),
		       GRID(20, 3, attrs.width),  GRID(5, 3, attrs.height));

	/* next */
	triangle[0].x = GRID(20, 16, attrs.width); triangle[0].y = GRID(5, 1, attrs.height);
	triangle[1].x = GRID(20, 16, attrs.width); triangle[1].y = GRID(5, 4, attrs.height);
	triangle[2].x = GRID(20, 19, attrs.width); triangle[2].y = GRID(2, 1, attrs.height);
	XFillPolygon(display, window, gc, triangle, 3, Convex, CoordModeOrigin);
}

int main(int argc, char **argv)
{
	XEvent xev;
	Atom delWindow;

	struct gbs *gbs;
	char *usercfg;
	struct termios ts;
	struct sigaction sa;

	i18n_init();

	/* initialize RNG */
	random_seed = time(0)+getpid();
	srand(random_seed);

	usercfg = get_userconfig(cfgfile);
	cfg_parse(SYSCONF_PREFIX "/gbsplayrc", options);
	cfg_parse((const char*)usercfg, options);
	free(usercfg);
	parseopts(&argc, &argv);
	select_plugin();

	if (argc < 1) {
		usage(1);
	}

	precalc_notes();
	precalc_vols();

	if (sound_open(endian, rate) != 0) {
		fprintf(stderr, _("Could not open output plugin \"%s\"\n"),
		        sound_name);
		exit(1);
	}

	if (sound_io)
		gbhw_setiocallback(iocallback, NULL);
	if (sound_write)
		gbhw_setcallback(callback, NULL);
	gbhw_setrate(rate);
	if (!gbhw_setfilter(filter_type)) {
		fprintf(stderr, _("Invalid filter type \"%s\"\n"), filter_type);
		exit(1);
	}

	if (argc >= 2) {
		sscanf(argv[1], "%ld", &subsong_start);
		subsong_start--;
	}

	if (argc >= 3) {
		sscanf(argv[2], "%ld", &subsong_stop);
		subsong_stop--;
	}

	gbs = gbs_open(argv[0]);
	if (gbs == NULL) {
		exit(1);
	}

	/* sanitize commandline values */
	if (subsong_start < -1) {
		subsong_start = 0;
	} else if (subsong_start >= gbs->songs) {
		subsong_start = gbs->songs-1;
	}
	if (subsong_stop <  0) {
		subsong_stop = -1;
	} else if (subsong_stop >= gbs->songs) {
		subsong_stop = -1;
	}
	
	gbs->subsong = subsong_start;
	gbs->subsong_timeout = subsong_timeout;
	gbs->silence_timeout = silence_timeout;
	gbs->gap = subsong_gap;
	gbs->fadeout = fadeout;
	setup_playmode(gbs);
	gbhw_setbuffer(&buf);
	gbs_set_nextsubsong_cb(gbs, nextsubsong_cb, NULL);
	gbs_init(gbs, gbs->subsong);
	if (sound_skip)
		sound_skip(gbs->subsong);
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

	display = XOpenDisplay(NULL);
	if (display == NULL) {
		fprintf(stderr, "Cannot open display\n");
		exit(1);
	}
	screen = DefaultScreen(display);
	window = XCreateSimpleWindow(display, RootWindow(display, screen),
				     10, 10, 200, 50, 0,
				     BlackPixel(display, screen),
				     BlackPixel(display, screen));

	gc = XCreateGC(display, window, 0, 0);
	XSetForeground(display, gc, WhitePixel(display, screen));

	delWindow = XInternAtom(display, "WM_DELETE_WINDOW", 0);
	XSetWMProtocols(display, window, &delWindow, 1);

	XSelectInput(display, window, ExposureMask | ButtonPressMask | ButtonReleaseMask);

	XMapWindow(display, window);

	while (!quit) {
		if (!gbs_step(gbs, refresh_delay)) {
			quit = 1;
			break;
		}

		while (XPending(display)) {
			XNextEvent(display, &xev);

			switch (xev.type) {

			case Expose:
				drawbuttons();
				break;

			case ButtonPress:
			case ClientMessage:
				quit = 1;
				break;
			}
		}

		updatetitle(gbs);
		handleuserinput(gbs);
	}

	XFreeGC(display, gc);
	XDestroyWindow(display, window);
	XCloseDisplay(display);

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &ots);

	sound_close();

	return 0;
}
