/*
 * gbsplay is a Gameboy sound player
 *
 * 2003-2005,2008,2018,2020 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                                 Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include "player.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#include "terminal.h"

/* lookup tables */
static char notelookup[4*MAXOCTAVE*12];
static char vollookup[5*16];
static const char vols[5] = " -=#%";

/* global variables */

static long quit = 0;
static long pause_mode = 0;

/* default values */
long redraw = false;

static long getnote(long div)
{
	long n = 0;

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

static char *reverse_vol(char *s)
{
	static char buf[5];
	long i;

	for (i=0; i<4; i++) {
		buf[i] = s[3-i];
	}
	buf[4] = 0;

	return buf;
}

static void precalc_vols(void)
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

static void stepcallback(long cycles, const struct gbhw_channel chan[], void *priv)
{
	sound_step(cycles, chan);
}

static void handleuserinput(struct gbs *gbs)
{
	char c;

	if (get_input(&c)) {
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

static char *notestring(long ch)
{
	long n;

	if (gbhw_ch[ch].mute) return "-M-";

	if (gbhw_ch[ch].volume == 0 ||
	    gbhw_ch[ch].master == 0 ||
	    (gbhw_ch[ch].leftgate == 0 &&
	     gbhw_ch[ch].rightgate == 0)) return "---";

	n = getnote(gbhw_ch[ch].div_tc);
	if (ch != 3) return &notelookup[4*n];
	else return "nse";
}

static long chvol(long ch)
{
	long v;

	if (gbhw_ch[ch].mute ||
	    gbhw_ch[ch].master == 0 ||
	    (gbhw_ch[ch].leftgate == 0 &&
	     gbhw_ch[ch].rightgate == 0)) return 0;

	if (ch == 2)
		v = (3-((gbhw_ch[2].volume+3)&3)) << 2;
	else v = gbhw_ch[ch].volume;

	return v;
}

static char *volstring(long v)
{
	if (v < 0) v = 0;
	if (v > 15) v = 15;

	return &vollookup[5*v];
}

static void printregs(void)
{
	long i;
	for (i=0; i<5*4; i++) {
		if (i % 5 == 0)
			printf("CH%ld:", i/5 + 1);
		printf(" %02x", gbhw_io_peek(0xff10+i));
		if (i % 5 == 4)
			printf("\n");
	}
	printf("MISC:");
	for (i+=0x10; i<0x27; i++) {
		printf(" %02x", gbhw_io_peek(0xff00+i));
	}
	printf("\nWAVE: ");
	for (i=0; i<16; i++) {
		printf("%02x", gbhw_io_peek(0xff30+i));
	}
	printf("\n\033[A\033[A\033[A\033[A\033[A\033[A");
}

static void printstatus(struct gbs *gbs)
{
	struct displaytime time;
	char *songtitle;

	update_displaytime(&time, gbs);

	songtitle = gbs->subsong_info[gbs->subsong].title;
	if (!songtitle) {
		songtitle=_("Untitled");
	}
	printf("\r\033[A\033[A"
	       "Song %3d/%3d (%s)\033[K\n"
	       "%02ld:%02ld/%02ld:%02ld",
	       gbs->subsong+1, gbs->songs, songtitle,
	       time.played_min, time.played_sec, time.total_min, time.total_sec);
	if (verbosity>2) {
		printf("  %s %s  %s %s  %s %s  %s %s  [%s|%s]\n",
		       notestring(0), volstring(chvol(0)),
		       notestring(1), volstring(chvol(1)),
		       notestring(2), volstring(chvol(2)),
		       notestring(3), volstring(chvol(3)),
		       reverse_vol(volstring(gbs->lvol/1024)),
		       volstring(gbs->rvol/1024));
	} else {
		puts("");
	}
	if (verbosity>3) {
		printregs();
	}
	fflush(stdout);
}

static void printinfo()
{
	if (verbosity>0) {
		puts(_("\ncommands:  [p]revious subsong   [n]ext subsong   [q]uit player\n" \
		         "           [ ] pause/resume   [1-4] mute channel"));
	}
	if (verbosity>1) {
		puts("\n\n"); /* additional newlines for the status display */
	}
	redraw = false;
}

int main(int argc, char **argv)
{
	struct gbs *gbs;

	gbs = common_init(argc, argv);

	/* set up terminal */
	printinfo();
	setup_terminal();

	/* init additional callbacks */
	if (sound_step)
		gbhw_setstepcallback(stepcallback, NULL);

	/* precalculate lookup tables */
	precalc_notes();
	precalc_vols();

	/* main loop */
	while (!quit) {
		if (!gbs_step(gbs, refresh_delay)) {
			quit = 1;
			break;
		}

		if (redraw) printinfo();
		if (verbosity>1) printstatus(gbs);
		handleuserinput(gbs);
	}

	/* stop sound */
	common_cleanup(gbs);

	/* clean up terminal */
	restore_terminal();
	if (verbosity>3) {
		printf("\n\n\n\n\n\n");
	}

	return 0;
}
