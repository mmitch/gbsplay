/* $Id: plugout_alsa.c,v 1.1 2006/12/20 20:51:28 ranmachan Exp $
 *
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
#include <alsa/asoundlib.h>

#include "plugout.h"

/* Handle for the PCM device */
snd_pcm_t *pcm_handle;

#if BYTE_ORDER == LITTLE_ENDIAN
#define SND_PCM_FORMAT_S16_NE SND_PCM_FORMAT_S16_LE
#else
#define SND_PCM_FORMAT_S16_NE SND_PCM_FORMAT_S16_BE
#endif

static long regparm alsa_open(enum plugout_endian endian, long rate)
{
	const char *pcm_name = "default";
	int fmt, err;
	unsigned exact_rate;
	snd_pcm_hw_params_t *hwparams;

	switch (endian) {
	case PLUGOUT_ENDIAN_BIG: fmt = SND_PCM_FORMAT_S16_BE; break;
	case PLUGOUT_ENDIAN_LITTLE: fmt = SND_PCM_FORMAT_S16_LE; break;
	default:
	case PLUGOUT_ENDIAN_NATIVE: fmt = SND_PCM_FORMAT_S16_NE; break;
	}

	snd_pcm_hw_params_alloca(&hwparams);

	if ((err = snd_pcm_open(&pcm_handle, pcm_name, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		fprintf(stderr, _("Could not open ALSA PCM device '%s': %s\n"), pcm_name, snd_strerror(err));
		return -1;
	}

	if ((err = snd_pcm_hw_params_any(pcm_handle, hwparams)) < 0) {
		fprintf(stderr, _("snd_pcm_hw_params_any failed: %s\n"), snd_strerror(err));
		return -1;
	}

	if ((err = snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf(stderr, _("snd_pcm_hw_params_set_access failed: %s\n"), snd_strerror(err));
		return -1;
	}

	if ((err = snd_pcm_hw_params_set_format(pcm_handle, hwparams, fmt)) < 0) {
		fprintf(stderr, _("snd_pcm_hw_params_set_format failed: %s\n"), snd_strerror(err));
		return -1;
	}

	exact_rate = rate;
	if ((err = snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams, &exact_rate, 0)) < 0) {
		fprintf(stderr, _("snd_pcm_hw_params_set_rate_near failed: %s\n"), snd_strerror(err));
		return -1;
	}
	if (rate != exact_rate) {
		fprintf(stderr, _("Requested rate %ldHz, got %dHz.\n"),
		        rate, exact_rate);
	}

	if ((err = snd_pcm_hw_params_set_channels(pcm_handle, hwparams, 2)) < 0) {
		fprintf(stderr, _("snd_pcm_hw_params_set_channels failed: %s\n"), snd_strerror(err));
		return -1;
	}

	if ((err = snd_pcm_hw_params_set_periods(pcm_handle, hwparams, 4, 0)) < 0) {
	      fprintf(stderr, _("snd_pcm_hw_params_set_periods failed: %s\n"), snd_strerror(err));
		return -1;
	}

	if ((err = snd_pcm_hw_params_set_buffer_size(pcm_handle, hwparams, 8192)) < 0) {
		fprintf(stderr, _("snd_pcm_hw_params_set_buffer_size failed: %s\n"), snd_strerror(err));
		return -1;
	}

	if ((err = snd_pcm_hw_params(pcm_handle, hwparams)) < 0) {
		fprintf(stderr, _("snd_pcm_hw_params failed: %s\n"), snd_strerror(err));
		return -1;
	}

	return 0;
}

static ssize_t regparm alsa_write(const void *buf, size_t count)
{
	snd_pcm_sframes_t retval;

	retval = snd_pcm_writei(pcm_handle, buf, count / 4);
	if (retval < 0) {
		fprintf(stderr, _("snd_pcm_writei failed: %s\n"), snd_strerror(retval));
		snd_pcm_prepare(pcm_handle);
	}
	return retval;
}

static void regparm alsa_close()
{
	snd_pcm_drop(pcm_handle);
	snd_pcm_close(pcm_handle);
}

const struct output_plugin plugout_alsa = {
	.name = "alsa",
	.description = "ALSA sound driver",
	.open = alsa_open,
	.write = alsa_write,
	.close = alsa_close,
};
