/*
 * gbsplay is a Gameboy sound player
 *
 * WAV file writer output plugin
 *
 * 2022 (C) by Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "filewriter.h"
#include "plugout.h"

static long chunk_size_offset;
static long data_subchunk_size_offset;
static long sample_rate;
static FILE* file = NULL;

static int wav_write_riff_header() {
	const char* riff_chunk_id = "RIFF";
	const char* wave_format = "WAVE";

	if (file_write_string(file, riff_chunk_id))
		return -1;

	if ((chunk_size_offset = file_write_32bit_placeholder(file)) == -1)
		return -1;

	if (file_write_string(file, wave_format))
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

	if (file_write_string(file, fmt_subchunk_id))
		return -1;

	if (file_write_32bit_le(file, fmt_subchunk_length))
		return -1;

	if (file_write_16bit_le(file, audio_format_uncompressed_pcm))
		return -1;

	if (file_write_16bit_le(file, num_channels))
		return -1;

	if (file_write_32bit_le(file, sample_rate))
		return -1;

	if (file_write_32bit_le(file, byte_rate))
		return -1;

	if (file_write_16bit_le(file, block_align))
		return -1;

	if (file_write_16bit_le(file, bits_per_sample))
		return -1;

	return 0;
}

static int wav_write_data_subchunk_header() {
	const char* data_subchunk_id = "data";

	if (file_write_string(file, data_subchunk_id))
		return -1;

	if ((data_subchunk_size_offset = file_write_32bit_placeholder(file)) == -1)
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
	const long filesize = file_get_position(file);
	if (filesize == -1)
		return -1;

	// chunk_size = total size - 8 bytes (RIFF reader + chunk size)
	if (file_write_32bit_le_at(file, filesize - 8, chunk_size_offset))
		return -1;

	// data subchunk size = total size - 44 bytes:
	// data subchunk size is written to bytes 40-43 and contains the length of data subchunk (that is everything until the the end of the file)
	if (file_write_32bit_le_at(file, filesize - 44, data_subchunk_size_offset))
		return -1;

	return 0;
}

static int wav_open_file(const int subsong) {
	if ((file = file_open("wav", subsong)) == NULL)
		return -1;

	if (wav_write_header() == -1)
		return -1;
	
	return 0;
}

static int wav_close_file() {
	int result;
	if (wav_update_header())
		return -1;

	result = file_close(file);
	file = NULL;
	return result;
}

static long wav_open(enum plugout_endian *endian,
		     const long rate, long *buffer_bytes)
{
	sample_rate = rate;

	*endian = PLUGOUT_ENDIAN_LITTLE;

	return 0;
}

static int wav_skip(const int subsong)
{
	if (file != NULL)
		if (wav_close_file())
			return -1;

	return wav_open_file(subsong);
}

static ssize_t wav_write(const void *buf, const size_t count)
{
	return file_write_bytes(file, buf, count);
}

static void wav_close()
{
	if (file != NULL)
		wav_close_file();

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
