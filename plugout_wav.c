/*
 * gbsplay is a Gameboy sound player
 *
 * WAV file writer output plugin
 *
 * 2022 (C) by Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include "common.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "plugout.h"

#define FILENAME_SIZE 23

static char filename[FILENAME_SIZE];
static FILE *file = NULL;

static long chunk_size_offset;
static long data_subchunk_size_offset;
static long sample_rate;
static int swap_audio_endianess;
static uint8_t *swap_buffer = NULL;

static int wav_write_bytes(const void *start, const size_t count) {
	return fwrite(start, 1, count, file) != count;
}

static int wav_write_string(const char* const string) {
	const unsigned long string_length = strlen(string);
	return wav_write_bytes(string, string_length);
}

static int wav_write_16bit_le(const uint16_t value) {
	uint8_t le[2];

	le[0] = value;
	le[1] = value >> 8;

	return wav_write_bytes(le, 2);
}

static int wav_write_32bit_le(const uint32_t value) {
	uint8_t le[4];

	le[0] = value;
	le[1] = value >> 8;
	le[2] = value >> 16;
	le[3] = value >> 24;

	return wav_write_bytes(le, 4);
}

static int wav_write_32bit_le_at(const uint32_t data, const long offset) {
	if (fseek(file, offset, SEEK_SET) == -1)
		return -1;
	return wav_write_32bit_le(data);
}

static long wav_write_32bit_placeholder() {
	long offset = ftell(file);
	if (offset == -1)
		return -1;

	if (wav_write_32bit_le(0))
		return -1;

	return offset;
}

static int wav_write_riff_header() {
	const char* riff_chunk_id = "RIFF";
	const char* wave_format = "WAVE";

	if (wav_write_string(riff_chunk_id))
		return -1;

	if ((chunk_size_offset = wav_write_32bit_placeholder()) == -1)
		return -1;

	if (wav_write_string(wave_format))
		return -1;

	return 0;
}

static int wav_write_fmt_subchunk() {
	const char* fmt_subchunk_id = "fmt ";
	const uint32_t fmt_subchunk_length = 16;

	const uint16_t audio_format_uncompressed_pcm = 1;
	const uint16_t num_channels = 2;
	const uint16_t bits_per_sample = 16;
	const uint32_t byte_rate = sample_rate * num_channels * bits_per_sample / 8;
	const uint16_t block_align = num_channels * bits_per_sample / 8;

	if (wav_write_string(fmt_subchunk_id))
		return -1;

	if (wav_write_32bit_le(fmt_subchunk_length))
		return -1;

	if (wav_write_16bit_le(audio_format_uncompressed_pcm))
		return -1;

	if (wav_write_16bit_le(num_channels))
		return -1;

	if (wav_write_32bit_le(sample_rate))
		return -1;

	if (wav_write_32bit_le(byte_rate))
		return -1;

	if (wav_write_16bit_le(block_align))
		return -1;

	if (wav_write_16bit_le(bits_per_sample))
		return -1;

	return 0;
}

static int wav_write_data_subchunk_header() {
	const char* data_subchunk_id = "data";

	if (wav_write_string(data_subchunk_id))
		return -1;

	if ((data_subchunk_size_offset = wav_write_32bit_placeholder()) == -1)
		return -1;

	return 0;
}

static int wav_write_header() {
	if (wav_write_riff_header())
		return -1;

	if (wav_write_fmt_subchunk())
		return -1;

	if (wav_write_data_subchunk_header())
		return -1;

	return 0;
}

static int wav_update_header() {
	const long filesize = ftell(file);
	if (filesize == -1)
		return -1;

	// chunk_size = total size - 8 bytes (RIFF reader + chunk size)
	if (wav_write_32bit_le_at(filesize - 8, chunk_size_offset))
		return -1;

	// data subchunk size = total size - 44 bytes:
	// data subchunk size is written to bytes 40-43 and contains the length of data subchunk (that is everything until the the end of the file)
	if (wav_write_32bit_le_at(filesize - 44, data_subchunk_size_offset))
		return -1;

	return 0;
}

static int wav_open_file(const int subsong) {
	if (snprintf(filename, FILENAME_SIZE, "gbsplay-%d.wav", subsong + 1) >= FILENAME_SIZE)
		goto error;

	if ((file = fopen(filename, "wb")) == NULL)
		goto error;

	if (wav_write_header())
		goto error;

	return 0;

error:
	if (file != NULL) {
		fclose(file);
		file = NULL;
	}
	return -1;
}

static int wav_close_file() {
	if (wav_update_header())
		return -1;

	return fclose(file);
}

static long wav_open(const enum plugout_endian endian,
		     const long rate, long *buffer_bytes)
{
	sample_rate = rate;

	switch (endian) {
	case PLUGOUT_ENDIAN_BIG:    swap_audio_endianess = true; break;
	case PLUGOUT_ENDIAN_LITTLE: swap_audio_endianess = false; break;
	default:
	case PLUGOUT_ENDIAN_NATIVE: swap_audio_endianess = is_be_machine(); break;
	}

	if (swap_audio_endianess) {
		swap_buffer = malloc(*buffer_bytes);
		if (swap_buffer == NULL)
			return -1;
	}

	return 0;
}

static int wav_skip(const int subsong)
{
	if (file)
		if (wav_close_file())
			return -1;

	return wav_open_file(subsong);
}

static ssize_t wav_write(const void *buf, const size_t count)
{
	size_t i;

	if (!swap_audio_endianess) {
		return wav_write_bytes(buf, count);
	}

	// BE->LE: swap all 16bit values in *buf
	for (i=0; i<count; i+=2) {
		swap_buffer[i]   = ((uint8_t*) buf)[i+1];
		swap_buffer[i+1] = ((uint8_t*) buf)[i];
	}
	return wav_write_bytes(swap_buffer, count);
}

static void wav_close()
{
	if (file != NULL)
		wav_close_file();

	if (swap_buffer != NULL)
		free(swap_buffer);

	return;
}

const struct output_plugin plugout_wav = {
	.name = "wav",
	.description = "WAV file writer",
	.open = wav_open,
	.skip = wav_skip,
	.write = wav_write,
	.close = wav_close,
};
