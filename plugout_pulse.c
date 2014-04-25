/*
 * gbsplay is a Gameboy sound player
 *
 * 2006 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *
 * Licensed under GNU GPL.
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <pulse/error.h>
#include <pulse/simple.h>

#include "plugout.h"

pa_simple *pulse_handle;
pa_sample_spec pulse_spec;

static long regparm pulse_open(enum plugout_endian endian, long rate)
{
	int err;

	switch (endian) {
	case PLUGOUT_ENDIAN_BIG: pulse_spec.format = PA_SAMPLE_S16BE; break;
	case PLUGOUT_ENDIAN_LITTLE: pulse_spec.format = PA_SAMPLE_S16LE; break;
	case PLUGOUT_ENDIAN_NATIVE: pulse_spec.format = PA_SAMPLE_S16NE; break;
	}
	pulse_spec.rate = rate;
	pulse_spec.channels = 2;

	pulse_handle = pa_simple_new(NULL, "gbsplay", PA_STREAM_PLAYBACK, NULL, "gbsplay", &pulse_spec, NULL, NULL, &err);
	if (!pulse_handle) {
		fprintf(stderr, "pulse: %s\n", pa_strerror(err));
		return -1;
	}

	return 0;
}

static ssize_t regparm pulse_write(const void *buf, size_t count)
{
	return pa_simple_write(pulse_handle, buf, count, NULL);
}

static void regparm pulse_close()
{
	pa_simple_free(pulse_handle);
}

const struct output_plugin plugout_pulse = {
	.name = "pulse",
	.description = "PulseAudio sound driver",
	.open = pulse_open,
	.write = pulse_write,
	.close = pulse_close,
};
