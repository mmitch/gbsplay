/*
 * gbsplay is a Gameboy sound player
 *
 * 2020 (C) by Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL v1 or, at your option, any later version.
 *
 * SDL2 sound output plugin
 */

#include "common.h"

#include <time.h>

#include <SDL2/SDL.h>

#include "plugout.h"

static const int PLAYBACK_MODE = 0;
static const int NO_CHANGES_ALLOWED = 0;
static const int UNPAUSE = 0;

static const struct timespec SLEEP_INTERVAL = {
	.tv_sec = 0,
	.tv_nsec = 1000
};

int device;

static long sdl_open(enum plugout_endian endian, long rate)
{
	SDL_AudioSpec desired, obtained;
	
	if (SDL_Init(SDL_INIT_AUDIO) != 0) {
		fprintf(stderr, _("Could not init SDL: %s\n"), SDL_GetError());
		return -1;
	}

	// TODO: what about endianess?
	SDL_zero(desired);
	desired.freq = rate;
	desired.format = AUDIO_S16;
	desired.channels = 2;
	desired.samples = 1024; // 4096;
	desired.callback = NULL;

	device = SDL_OpenAudioDevice(NULL, PLAYBACK_MODE, &desired, &obtained, NO_CHANGES_ALLOWED);
	if (device == 0) {
		fprintf(stderr, _("Could not open SDL audio device: %s\n"), SDL_GetError());
		return -1;
	}

	SDL_PauseAudioDevice(device, UNPAUSE);

	return 0;
}

static ssize_t sdl_write(const void *buf, size_t count)
{
	while (SDL_GetQueuedAudioSize(device) > count)
		nanosleep(&SLEEP_INTERVAL, NULL);

	if (SDL_QueueAudio(device, buf, count) != 0) {
		fprintf(stderr, _("Could not write SDL audio data: %s\n"), SDL_GetError());
		return -1;
	}
	return count;
}

static void sdl_close()
{
	SDL_Quit();
}

const struct output_plugin plugout_sdl = {
	.name = "sdl",
	.description = "SDL sound driver",
	.open = sdl_open,
	.write = sdl_write,
	.close = sdl_close,
};
