/*
 * gbsplay is a Gameboy sound player
 * This file contains the player code common to both CLI and X11 frontends.
 *
 * 2003-2005,2008,2018,2020 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                                 Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include "common.h"

#include <stdlib.h>

#include "util.h"

#include "player.h"

/* global variables */
char *myname;
long quit = 0;
static long *subsong_playlist;
static long subsong_playlist_idx = 0;
long pause_mode = 0;

unsigned long random_seed;

#define DEFAULT_REFRESH_DELAY 33

long refresh_delay = DEFAULT_REFRESH_DELAY; /* msec */

/* default values */
long playmode = PLAYMODE_LINEAR;
long loopmode = 0;
enum plugout_endian endian = PLUGOUT_ENDIAN_NATIVE;
long rate = 44100;
long silence_timeout = 2;
long fadeout = 3;
long subsong_gap = 2;
long subsong_start = -1;
long subsong_stop = -1;
long subsong_timeout = 2*60;

const char cfgfile[] = ".gbsplayrc";

char *sound_name = PLUGOUT_DEFAULT;
char *filter_type = GBHW_CFG_FILTER_DMG;
char *sound_description;
plugout_open_fn  sound_open;
plugout_skip_fn  sound_skip;
plugout_pause_fn sound_pause;
plugout_io_fn    sound_io;
plugout_write_fn sound_write;
plugout_close_fn sound_close;

static int16_t samples[4096];
struct gbhw_buffer buf = {
	.data = samples,
	.pos  = 0,
	.bytes = sizeof(samples),
};

regparm void swap_endian(struct gbhw_buffer *buf)
{
	long i;

	for (i=0; i<buf->bytes/sizeof(short); i++) {
		short x = buf->data[i];
		buf->data[i] = ((x & 0xff) << 8) | (x >> 8);
	}
}

regparm void iocallback(long cycles, uint32_t addr, uint8_t val, void *priv)
{
	sound_io(cycles, addr, val);
}

regparm void callback(struct gbhw_buffer *buf, void *priv)
{
	if ((is_le_machine() && endian == PLUGOUT_ENDIAN_BIG) ||
	    (is_be_machine() && endian == PLUGOUT_ENDIAN_LITTLE)) {
		swap_endian(buf);
	}
	sound_write(buf->data, buf->pos*2*sizeof(int16_t));
	buf->pos = 0;
}

regparm long *setup_playlist(long songs)
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

regparm long get_next_subsong(struct gbs *gbs)
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

regparm int get_prev_subsong(struct gbs *gbs)
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

regparm void setup_playmode(struct gbs *gbs)
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

regparm long nextsubsong_cb(struct gbs *gbs, void *priv)
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

regparm void version(void)
{
	printf("%s %s\n", myname, GBS_VERSION);
	exit(0);
}
