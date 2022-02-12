/*
 * gbsplay is a Gameboy sound player
 *
 * 2003-2020 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                  Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#include "player.h"
#include "terminal.h"

/* lookup tables */
static char notelookup[4*MAXOCTAVE*12];
static char vollookup[5*16];
static const char vols[5] = " -=#%";

/* global variables */

static long quit = 0;

/* default values */
long redraw = false;

/* forward declarations */
static void printstatus(struct gbs *gbs);
static void printinfo();

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

static void stepcallback(struct gbs *gbs, cycles_t cycles, const struct gbs_channel_status chan[], void *priv)
{
	sound_step(cycles, chan);
}

static void handleuserinput(struct gbs *gbs)
{
	char c;

	if (get_input(&c)) {
		switch (c) {
		case 'p':
			play_prev_subsong(gbs);
			break;
		case 'n':
			play_next_subsong(gbs);
			break;
		case 'q':
		case 27:
			quit = 1;
			break;
		case ' ':
			toggle_pause(gbs);
			if (redraw) printinfo();
			if (verbosity>1) printstatus(gbs);
			break;
		case 'l':
			gbs_cycle_loop_mode(gbs);
			break;
		case '1':
		case '2':
		case '3':
		case '4':
			gbs_toggle_mute(gbs, c-'1');
			break;
		}
	}
}

// TODO: only pass struct gbhw_channnel instead of struct gbhw?
static char *notestring(const struct gbs_status *status, long ch)
{
	long n;

	if (status->ch[ch].mute) return "-M-";

	if (status->ch[ch].vol == 0) return "---";

	n = getnote(status->ch[ch].div_tc);
	if (ch != 3) return &notelookup[4*n];
	else return "nse";
}

static char *volstring(long v)
{
	if (v < 0) v = 0;
	if (v > 15) v = 15;

	return &vollookup[5*v];
}

static char *loopmodestring(const struct gbs_status *status)
{
	switch (status->loop_mode) {
	default:
	case LOOP_OFF:    return "";
	case LOOP_RANGE:  return _(" [loop range]");
	case LOOP_SINGLE: return _(" [loop single]");
	}
}

static void printregs(struct gbs *gbs)
{
	long i;
	for (i=0; i<5*4; i++) {
		if (i % 5 == 0)
			printf("CH%ld:", i/5 + 1);
		printf(" %02x", gbs_io_peek(gbs, 0xff10+i));
		if (i % 5 == 4)
			printf("\n");
	}
	printf("MISC:");
	for (i+=0x10; i<0x27; i++) {
		printf(" %02x", gbs_io_peek(gbs, 0xff00+i));
	}
	printf("\nWAVE: ");
	for (i=0; i<16; i++) {
		printf("%02x", gbs_io_peek(gbs, 0xff30+i));
	}
	printf("\n\033[A\033[A\033[A\033[A\033[A\033[A");
}

static void printstatus(struct gbs *gbs)
{
	const struct gbs_status *status;
	struct displaytime time;
	long pausemode;

	status = gbs_get_status(gbs);
	pausemode = get_pause();

	update_displaytime(&time, status);

	printf("\r\033[A\033[A"
	       "Song %3d/%3d%s%s (%s)\033[K\n"
	       "%02ld:%02ld/%02ld:%02ld",
	       status->subsong+1, status->songs, pausemode ? " [Paused]" : "",
	       loopmodestring(status), status->songtitle,
	       time.played_min, time.played_sec, time.total_min, time.total_sec);
	if (verbosity>2) {
		printf("  %s %s  %s %s  %s %s  %s %s  [%s|%s]\n",
		       notestring(status, 0), volstring(status->ch[0].vol),
		       notestring(status, 1), volstring(status->ch[1].vol),
		       notestring(status, 2), volstring(status->ch[2].vol),
		       notestring(status, 3), volstring(status->ch[3].vol),
		       reverse_vol(volstring(status->lvol/1024)),
		       volstring(status->rvol/1024));
	} else {
		puts("");
	}
	if (verbosity>3) {
		printregs(gbs);
	}
	fflush(stdout);
}

static void printinfo()
{
	if (verbosity>0) {
		puts(_("\ncommands:  [p]revious subsong   [n]ext subsong   [q]uit player\n" \
		         "           [ ] pause/resume   [1-4] mute channel   [l]oop mode"));
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
		gbs_set_step_callback(gbs, stepcallback, NULL);

	/* precalculate lookup tables */
	precalc_notes();
	precalc_vols();

	/* main loop */
	while (!quit) {
		if (!step_emulation(gbs)) {
			quit = 1;
			break;
		}
		if (is_running()) {
			if (redraw) printinfo();
			if (verbosity>1) printstatus(gbs);
		}
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
