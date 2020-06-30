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
#include <time.h>

#include "cfgparser.h"
#include "terminal.h"

/* lookup tables */
static char notelookup[4*MAXOCTAVE*12];
static char vollookup[5*16];
static const char vols[5] = " -=#%";

/* global variables */

/* default values */
static long verbosity = 3;
long redraw = false;
static plugout_step_fn sound_step;

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
	{ "verbosity", &verbosity, cfg_long },
	/* playmode not implemented yet */
	{ NULL, NULL, NULL }
};

static regparm long getnote(long div)
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

static regparm char *reverse_vol(char *s)
{
	static char buf[5];
	long i;

	for (i=0; i<4; i++) {
		buf[i] = s[3-i];
	}
	buf[4] = 0;

	return buf;
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

static regparm void stepcallback(long cycles, const struct gbhw_channel chan[], void *priv)
{
	sound_step(cycles, chan);
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
		  "  -q        reduce verbosity\n"
		  "  -r        set samplerate (%ldHz)\n"
		  "  -R        set refresh delay (%ld milliseconds)\n"
		  "  -t        set subsong timeout (%ld seconds)\n"
		  "  -T        set silence timeout (%ld seconds)\n"
		  "  -v        increase verbosity\n"
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
	while ((res = getopt(*argc, *argv, "1234c:E:f:g:hH:lo:qr:R:t:T:vVzZ")) != -1) {
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
		case 'q':
			verbosity -= 1;
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
		case 'v':
			verbosity += 1;
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

static regparm char *notestring(long ch)
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

static regparm long chvol(long ch)
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

static regparm char *volstring(long v)
{
	if (v < 0) v = 0;
	if (v > 15) v = 15;

	return &vollookup[5*v];
}

static regparm void printregs(void)
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

static regparm void printstatus(struct gbs *gbs)
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
	printf("\r\033[A\033[A"
	       "Song %3d/%3d (%s)\033[K\n"
	       "%02ld:%02ld/%02ld:%02ld",
	       gbs->subsong+1, gbs->songs, songtitle,
	       timem, times, lenm, lens);
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

static regparm void printinfo(struct gbs *gbs)
{
	if (verbosity>0) {
		gbs_printinfo(gbs, 0);
		puts(_("\ncommands:  [p]revious subsong   [n]ext subsong   [q]uit player\n" \
		         "           [ ] pause/resume   [1-4] mute channel"));
	}
	if (verbosity>1) {
		puts("\n\n"); /* additional newlines for the status display */
	}
	redraw = false;
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
	sound_step = plugout->step;
	sound_write = plugout->write;
	sound_close = plugout->close;
	sound_pause = plugout->pause;
	sound_description = plugout->description;

	if (plugout->flags & PLUGOUT_USES_STDOUT) {
		verbosity = 0;
	}
}

int main(int argc, char **argv)
{
	struct gbs *gbs;
	char *usercfg;

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
	if (sound_step)
		gbhw_setstepcallback(stepcallback, NULL);
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
	printinfo(gbs);

	setup_terminal();
	while (!quit) {
		if (!gbs_step(gbs, refresh_delay)) {
			quit = 1;
			break;
		}

		if (redraw) printinfo(gbs);
		if (verbosity>1) printstatus(gbs);
		handleuserinput(gbs);
	}
	restore_terminal();

	sound_close();

	if (verbosity>3) {
		printf("\n\n\n\n\n\n");
	}

	return 0;
}
