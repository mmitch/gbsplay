/*
 * xgbsplay is an X11 frontend for gbsplay, the Gameboy sound player
 *
 * 2003-2005,2008,2018,2020 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                                 Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include "player.h"

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <libgen.h>

#include <X11/Xlib.h>

#include "cfgparser.h"

#define GRID(s,t,i) ((t * i / s))

/* global variables */
#define STATUSTEXT_LENGTH 256
static char statustext[STATUSTEXT_LENGTH];
static char oldstatustext[STATUSTEXT_LENGTH];
static char filename[48];

static Display *display;
static Window window;
static int screen;
static GC gc;

static regparm void updatetitle(struct gbs *gbs)
{
	long time = gbs->ticks / GBHW_CLOCK;
	long timem = time / 60;
	long times = time % 60;
	long len = gbs->subsong_info[gbs->subsong].len / 1024;
	long lenm, lens;

	if (len == 0) {
		len = subsong_timeout;
	}
	lenm = len / 60;
	lens = len % 60;

	snprintf(statustext, STATUSTEXT_LENGTH, /* or use sizeof(statustext) ?? */
		 "xgbsplay %s %d/%d "
		 "%02ld:%02ld/%02ld:%02ld",
		 filename, gbs->subsong+1, gbs->songs,
		 timem, times, lenm, lens);

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
	exit(1);
}

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
	line[0].x2 = line[0].x1;
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

static regparm int handlebutton(XButtonEvent *xev, struct gbs *gbs)
{
	XWindowAttributes attrs;

	XGetWindowAttributes(display, window, &attrs);

	switch (xev->x * 4 / attrs.width) {
	case 0: /* prev */
		gbs->subsong = get_prev_subsong(gbs);
		while (gbs->subsong < 0) {
			gbs->subsong += gbs->songs;
		}
		gbs_init(gbs, gbs->subsong);
		if (sound_skip)
			sound_skip(gbs->subsong);
		break;

	case 1: /* pause */
		pause_mode = !pause_mode;
		gbhw_pause(pause_mode);
		if (sound_pause) sound_pause(pause_mode);
		break;

	case 2: /* stop */
		return 1;

	case 3: /* next */
		gbs->subsong = get_next_subsong(gbs);
		gbs->subsong %= gbs->songs;
		gbs_init(gbs, gbs->subsong);
		if (sound_skip)
			sound_skip(gbs->subsong);
		break;
	}

	return 0;
}

int main(int argc, char **argv)
{
	XEvent xev;
	Atom delWindow;

	struct gbs *gbs;
	char *usercfg;
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
	strncpy(filename, basename(argv[0]), sizeof(filename));
	filename[sizeof(filename) - 1] = '\0';

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
	if (verbosity>0) {
		gbs_printinfo(gbs, 0);
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = exit_handler;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGSEGV, &sa, NULL);

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
				if (handlebutton((XButtonEvent *)&xev, gbs))
					quit = 1;
				break;

			case ClientMessage:
				quit = 1;
				break;
			}
		}

		updatetitle(gbs);
	}

	XFreeGC(display, gc);
	XDestroyWindow(display, window);
	XCloseDisplay(display);

	sound_close();

	return 0;
}
