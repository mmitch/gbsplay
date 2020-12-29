/*
 * xgbsplay is an X11 frontend for gbsplay, the Gameboy sound player
 *
 * 2003-2020 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                  Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include "player.h"

#include <stdlib.h>
#include <X11/Xlib.h>

#define GRID(s,t,i) ((t * i / s))

/* global variables */
#define STATUSTEXT_LENGTH 256
static char statustext[STATUSTEXT_LENGTH];
static char oldstatustext[STATUSTEXT_LENGTH];

static long quit = 0;

static Display *display;
static Window window;
static int screen;
static GC gc;

static void updatetitle(struct gbs *gbs)
{
	const struct gbs_status *status = gbs_get_status(gbs);
	struct displaytime time;

	update_displaytime(&time, status);

	snprintf(statustext, STATUSTEXT_LENGTH, /* or use sizeof(statustext) ?? */
		 "xgbsplay %s %d/%d "
		 "%02ld:%02ld/%02ld:%02ld",
		 filename, status->subsong+1, status->songs,
		 time.played_min, time.played_sec, time.total_min, time.total_sec);

	if (strncmp(statustext, oldstatustext, STATUSTEXT_LENGTH) != 0) {  /* or use sizeof(statustext) ?? or strcmp() ?? */
		strncpy(oldstatustext, statustext, STATUSTEXT_LENGTH);
		XStoreName(display, window, statustext);
	}
}

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

static int handlebutton(XButtonEvent *xev, struct gbs *gbs)
{
	XWindowAttributes attrs;

	XGetWindowAttributes(display, window, &attrs);

	switch (xev->x * 4 / attrs.width) {
	case 0: /* prev */
		play_prev_subsong(gbs);
		break;

	case 1: /* pause */
		toggle_pause(gbs);
		break;

	case 2: /* stop */
		return 1;

	case 3: /* next */
		play_next_subsong(gbs);
		break;
	}

	return 0;
}

int main(int argc, char **argv)
{
	XEvent xev;
	Atom delWindow;

	struct gbs *gbs;
	struct sigaction sa;

	gbs = common_init(argc, argv);

	/* init signal handlers */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = exit_handler;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGSEGV, &sa, NULL);

	/* init X11 */
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

	/* main loop */
	while (!quit) {
		if (!step_emulation(gbs)) {
			quit = 1;
			break;
		}
		if (is_running())
			updatetitle(gbs);

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
	}

	/* stop sound */
	common_cleanup(gbs);

	/* clean up X11 stuff */
	XFreeGC(display, gc);
	XDestroyWindow(display, window);
	XCloseDisplay(display);

	return 0;
}
