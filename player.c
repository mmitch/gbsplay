/*
 * gbsplay is a Gameboy sound player
 *
 * This file contains the player code common to both CLI and X11 frontends.
 *
 * 2003-2025 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                  Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <strings.h>

#include "common.h"
#include "util.h"
#include "cfgparser.h"
#include "libgbs.h"
#include "gbs_internal.h"
#include "player.h"

/* global variables */
char *myname;
char *filename;

struct filter_map {
	char *name;
	enum gbs_filter_type type;
};

const struct filter_map FILTERS[] = {
	{ CFG_FILTER_OFF, FILTER_OFF },
	{ CFG_FILTER_DMG, FILTER_DMG },
	{ CFG_FILTER_CGB, FILTER_CGB },
	{ NULL, -1 },
};

static long *subsong_playlist;
static long subsong_playlist_idx = 0;
static long pause_mode = 0;

static unsigned long random_seed;

plugout_open_fn  sound_open;
plugout_skip_fn  sound_skip;
plugout_pause_fn sound_pause;
plugout_io_fn    sound_io;
plugout_step_fn  sound_step;
plugout_write_fn sound_write;
plugout_close_fn sound_close;

static enum plugout_endian actual_endian = PLUGOUT_ENDIAN_AUTOSELECT;
static long subsong_start = -1;
static long subsong_stop = -1;

static long mute_channel[4];

static const char cfgfile[] = ".gbsplayrc";

static char *sound_description;

static struct gbs_output_buffer buf = {
	.data = NULL,
	.bytes = 8192,
	.pos  = 0,
};

static struct timespec pause_wait_time;

static void swap_endian(struct gbs_output_buffer *buf)
{
	unsigned long i;

	for (i=0; i<buf->bytes/sizeof(uint16_t); i++) {
		uint16_t x = buf->data[i];
		buf->data[i] = ((x & 0xff) << 8) | (x >> 8);
	}
}

static void iocallback(struct gbs *gbs, cycles_t cycles, uint32_t addr, uint8_t value, void *priv)
{
	UNUSED(gbs);
	UNUSED(priv);

	sound_io(cycles, addr, value);
}

static void callback(struct gbs *gbs, struct gbs_output_buffer *buf, void *priv)
{
	UNUSED(gbs);
	UNUSED(priv);

	if (actual_endian != PLUGOUT_ENDIAN_NATIVE) {
		swap_endian(buf);
	}
	sound_write(buf->data, buf->pos*2*sizeof(int16_t));
	buf->pos = 0;
}

long *setup_playlist(long songs)
/* setup a playlist in shuffle mode */
{
	long i;
	long *playlist;

	playlist = (long*) calloc( songs, sizeof(long) );
	for (i=0; i<songs; i++) {
		playlist[i] = i;
	}

	/* reinit RNG with current seed - playlists shall be reproducible! */
	rand_seed(random_seed);
	shuffle_long(playlist, songs);

	return playlist;
}

static long get_next_subsong(struct gbs *gbs)
/* returns the number of the subsong that is to be played next */
{
	const struct gbs_status *status = gbs_get_status(gbs);
	long next = -1;

	switch (cfg.play_mode) {

	case PLAY_MODE_RANDOM:
		next = rand_long(status->songs);
		break;

	case PLAY_MODE_SHUFFLE:
		subsong_playlist_idx++;
		if (subsong_playlist_idx == status->songs) {
			free(subsong_playlist);
			random_seed++;
			subsong_playlist = setup_playlist(status->songs);
			subsong_playlist_idx = 0;
		}
		next = subsong_playlist[subsong_playlist_idx];
		break;

	case PLAY_MODE_LINEAR:
		next = status->subsong + 1;
		break;
	}

	return next;
}

static long get_prev_subsong(struct gbs *gbs)
/* returns the number of the subsong that has been played previously */
{
	const struct gbs_status *status = gbs_get_status(gbs);
	long prev = -1;

	switch (cfg.play_mode) {

	case PLAY_MODE_RANDOM:
		prev = rand_long(status->songs);
		break;

	case PLAY_MODE_SHUFFLE:
		subsong_playlist_idx--;
		if (subsong_playlist_idx == -1) {
			free(subsong_playlist);
			random_seed--;
			subsong_playlist = setup_playlist(status->songs);
			subsong_playlist_idx = status->songs-1;
		}
		prev = subsong_playlist[subsong_playlist_idx];
		break;

	case PLAY_MODE_LINEAR:
		prev = status->subsong - 1;
		break;
	}

	return prev;
}

static void play_subsong(struct gbs *gbs, long subsong) {
	/*
	 * sound_skip notifies the plugout we are preparing to play the next
	 * subsong.
	 */
	if (sound_skip)
		sound_skip(subsong);
	/*
	 * Now that the plugout is notified we can re-initialize the GBS state.
	 */
	gbs_init(gbs, subsong);
}

void play_next_subsong(struct gbs *gbs)
{
	const struct gbs_status *status = gbs_get_status(gbs);
	long next = get_next_subsong(gbs);
	next %= status->songs;
	play_subsong(gbs, next);
}

void play_prev_subsong(struct gbs *gbs)
{
	const struct gbs_status *status = gbs_get_status(gbs);
	long prev = get_prev_subsong(gbs);
	while (prev < 0)
		prev += status->songs;
	play_subsong(gbs, prev);
}

static long setup_play_mode(struct gbs *gbs)
/* initializes the chosen play mode (set start subsong etc.) */
{
	const struct gbs_status *status = gbs_get_status(gbs);
	long subsong = status->subsong;

	switch (cfg.play_mode) {

	case PLAY_MODE_RANDOM:
		if (subsong == -1) {
			subsong = get_next_subsong(gbs);
		}
		break;

	case PLAY_MODE_SHUFFLE:
		subsong_playlist = setup_playlist(status->songs);
		subsong_playlist_idx = 0;
		if (subsong == -1) {
			subsong = subsong_playlist[0];
		} else {
			/* randomize playlist until desired start song is first */
			/* (rotation does not work because this must be reproducible */
			/* by setting random_seed to the old value */
			while (subsong_playlist[0] != subsong) {
				random_seed++;
				subsong_playlist = setup_playlist(status->songs);
			}
		}
		break;

	case PLAY_MODE_LINEAR:
		if (subsong == -1) {
			subsong = status->defaultsong - 1;
		}
		break;
	}

	return subsong;
}

long nextsubsong_cb(struct gbs *gbs, void *priv)
{
	const struct gbs_status *status = gbs_get_status(gbs);
	long subsong = get_next_subsong(gbs);

	UNUSED(priv);

	if (status->loop_mode == LOOP_SINGLE) {
		subsong = status->subsong;
	} else if (status->subsong == subsong_stop || subsong >= status->songs) {
		if (status->loop_mode == LOOP_OFF) {
			return false;
		}
		subsong = subsong_start;
		setup_play_mode(gbs);
	}

	play_subsong(gbs, subsong);
	return true;
}

long is_running() {
	return !pause_mode;
}

void toggle_pause()
{
	pause_mode = !pause_mode;
	if (sound_pause)
		sound_pause(pause_mode);
}

const char *get_pause_string()
{
	return pause_mode ? _(" [paused]") : "";
}

const char *get_loopmode_string(const struct gbs_status *status)
{
	switch (status->loop_mode) {
	default:
	case LOOP_OFF:    return "";
	case LOOP_RANGE:  return _(" [loop range]");
	case LOOP_SINGLE: return _(" [loop single]");
	}
}

long step_emulation(struct gbs *gbs) {
	if (is_running()) {
		return gbs_step(gbs, cfg.refresh_delay);
	}

	nanosleep(&pause_wait_time, NULL);
	return true;
}

void update_displaytime(struct displaytime *time, const struct gbs_status *status)
{
	long played = status->ticks / GBHW_CLOCK;
	long total = status->subsong_len / 1024;

	time->played_min = played / 60;
	time->played_sec = played % 60;

	if (total != 0) {
		time->total_min = total / 60;
		time->total_sec = total % 60;
	} else {
		time->total_min = 99;
		time->total_sec = 99;
	}
}

static char *endian_str(long endian)
{
	switch (endian) {
	case PLUGOUT_ENDIAN_BIG: return _("big");
	case PLUGOUT_ENDIAN_LITTLE: return _("little");
	case PLUGOUT_ENDIAN_AUTOSELECT: return _("native");
	default: return _("invalid");
	}
}

static char *filename_only(char *with_pathname)
/* this creates an additional reference the original string, so don't free it */
{
	char *ret = with_pathname;
	char *p   = with_pathname;

	/* we could strlen() and then iterate backwards,
           but we'd have to iterate over the string twice */
	while (*p != 0) {
		if (*p++ == '/')
			ret = p;
	}
	return ret;
}

static void version(void)
{
	printf("%s %s\n", myname, GBS_VERSION);
	exit(0);
}

static void usage(long exitcode)
{
	FILE *out = exitcode ? stderr : stdout;
	fprintf(out,
		_("Usage: %s [OPTION]... [--] GBS-FILE [START-AT-SUBSONG [STOP-AFTER-SUBSONG] ]\n"
		  "\n"
		  "Available options are:\n"
		  "  -E        endian, b == big, l == little, n == native (%s)\n"
		  "  -f        set fadeout (%ld seconds)\n"
		  "  -g        set subsong gap (%ld seconds)\n"
		  "  -h        display this help and exit\n"
		  "  -H        set output high-pass type (%s)\n"
		  "  -l        set loop mode to range\n"
		  "  -L        set loop mode to single\n"
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
		  "  -1 to -4  mute a channel on startup\n"
		  "  --        end options, next argument is GBS-FILE\n"),
		myname,
		endian_str(cfg.requested_endian),
		cfg.fadeout,
		cfg.subsong_gap,
		_(cfg.filter_type),
		cfg.sound_name,
		cfg.rate,
		cfg.refresh_delay,
		cfg.subsong_timeout,
		cfg.silence_timeout);
	exit(exitcode);
}

static void parseopts(int *argc, char ***argv)
{
	long res;
	myname = filename_only(*argv[0]);
	while ((res = getopt(*argc, *argv, "1234c:E:f:g:hH:lLo:qr:R:t:T:vVzZ")) != -1) {
		switch (res) {
		default:
			usage(1);
			break;
		case '1':
		case '2':
		case '3':
		case '4':
			mute_channel[res-'1'] ^= 1;
			break;
		case 'c':
			cfg_parse(optarg);
			break;
		case 'E':
			if (strcasecmp(optarg, "b") == 0) {
				cfg.requested_endian = PLUGOUT_ENDIAN_BIG;
			} else if (strcasecmp(optarg, "l") == 0) {
				cfg.requested_endian = PLUGOUT_ENDIAN_LITTLE;
			} else if (strcasecmp(optarg, "n") == 0) {
				cfg.requested_endian = PLUGOUT_ENDIAN_NATIVE;
			} else {
				printf(_("\"%s\" is not a valid endian.\n\n"), optarg);
				usage(1);
			}
			break;
		case 'f':
			sscanf(optarg, "%ld", &cfg.fadeout);
			break;
		case 'g':
			sscanf(optarg, "%ld", &cfg.subsong_gap);
			break;
		case 'h':
			usage(0);
			break;
		case 'H':
			cfg.filter_type = optarg;
			break;
		case 'l':
			cfg.loop_mode = LOOP_RANGE;
			break;
		case 'L':
			cfg.loop_mode = LOOP_SINGLE;
			break;
		case 'o':
			cfg.sound_name = optarg;
			break;
		case 'q':
			cfg.verbosity -= 1;
			break;
		case 'r':
			sscanf(optarg, "%ld", &cfg.rate);
			break;
		case 'R':
			sscanf(optarg, "%ld", &cfg.refresh_delay);
			break;
		case 't':
			sscanf(optarg, "%ld", &cfg.subsong_timeout);
			break;
		case 'T':
			sscanf(optarg, "%ld", &cfg.silence_timeout);
			break;
		case 'v':
			cfg.verbosity += 1;
			break;
		case 'V':
			version();
			break;
		case 'z':
			cfg.play_mode = PLAY_MODE_SHUFFLE;
			break;
		case 'Z':
			cfg.play_mode = PLAY_MODE_RANDOM;
			break;
		}
	}
	*argc -= optind;
	*argv += optind;
}

static void select_plugin(void)
{
	const struct output_plugin *plugout;

	if (strcmp(cfg.sound_name, "list") == 0) {
		plugout_list_plugins();
		exit(0);
	}

	plugout = plugout_select_by_name(cfg.sound_name);
	if (plugout == NULL) {
		fprintf(stderr, _("\"%s\" is not a known output plugin.\n\n"),
		        cfg.sound_name);
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
		cfg.verbosity = 0;
	}
}

static enum gbs_filter_type parse_filter(const char *filter_name) {
	for (const struct filter_map *filter = FILTERS; filter->name != NULL; filter++) {
		if (strcasecmp(filter_name, filter->name) == 0) {
			return filter->type;
		}
	}
	return -1;
}

struct gbs *common_init(int argc, char **argv)
{
	char *usercfg;
	struct gbs *gbs;
	uint8_t songs;
	uint8_t initial_subsong;
	struct plugout_metadata metadata;

	i18n_init();

	/* initialize RNG */
	random_seed = time(0)+getpid();
	rand_seed(random_seed);

	usercfg = get_userconfig(cfgfile);
	cfg_parse(SYSCONF_PREFIX "/gbsplayrc");
	cfg_parse((const char*)usercfg);
	free(usercfg);
	parseopts(&argc, &argv);
	select_plugin();

	if (argc < 1) {
		usage(1);
	}

	/* options have been parsed, argv[0] is our filename */
	filename = filename_only(argv[0]);

	if (cfg.requested_endian == PLUGOUT_ENDIAN_AUTOSELECT) {
		actual_endian = PLUGOUT_ENDIAN_NATIVE;
	} else {
		actual_endian = cfg.requested_endian;
	}

	metadata.player_name = myname;
	metadata.filename = filename;
	if (sound_open(&actual_endian, cfg.rate, &buf.bytes, metadata) != 0) {
		fprintf(stderr, _("Could not open output plugin \"%s\"\n"),
		        cfg.sound_name);
		exit(1);
	}
	if (cfg.requested_endian != PLUGOUT_ENDIAN_AUTOSELECT &&
	    actual_endian != cfg.requested_endian) {
		fprintf(stderr, _("Unsupported endian for output plugin \"%s\"\n"),
		        cfg.sound_name);
		exit(1);
	}
	buf.data = malloc(buf.bytes);

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

	if (sound_io)
		gbs_set_io_callback(gbs, iocallback, NULL);
	if (sound_write)
		gbs_set_sound_callback(gbs, callback, NULL);
	gbs_configure_output(gbs, &buf, cfg.rate);
	if (!gbs_set_filter(gbs, parse_filter(cfg.filter_type))) {
		fprintf(stderr, _("Invalid filter type \"%s\"\n"), cfg.filter_type);
		exit(1);
	}

	/* sanitize commandline values */
	songs = gbs_get_status(gbs)->songs;
	if (subsong_start < -1) {
		subsong_start = 0;
	} else if (subsong_start >= songs) {
		subsong_start = songs-1;
	}
	if (subsong_stop <  0) {
		subsong_stop = -1;
	} else if (subsong_stop >= songs) {
		subsong_stop = -1;
	}

	/* convert delay to internal wait */
	pause_wait_time.tv_nsec = cfg.refresh_delay * 1000000;

	// FIXME: proper configuration interface to gbs, this is just quickly slapped together
	gbs_configure(gbs, subsong_start, cfg.subsong_timeout, cfg.silence_timeout, cfg.subsong_gap, cfg.fadeout);
	gbs_set_loop_mode(gbs, cfg.loop_mode);
	gbs_configure_channels(gbs, mute_channel[0], mute_channel[1], mute_channel[2], mute_channel[3]);

	gbs_set_nextsubsong_cb(gbs, nextsubsong_cb, NULL);
	initial_subsong = setup_play_mode(gbs);
	play_subsong(gbs, initial_subsong);
	if (cfg.verbosity>0) {
		gbs_internal_api.print_info(gbs, 0);
	}

	return gbs;
}

void common_cleanup(struct gbs *gbs)
{
	sound_close();
	free(buf.data);
	gbs_close(gbs);
}
