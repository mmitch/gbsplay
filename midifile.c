/*
 * gbsplay is a Gameboy sound player
 *
 * common code of MIDI output plugins
 *
 * 2006-2022 (C) by Christian Garbs <mitch@cgarbs.de>
 *                  Vegard Nossum
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include "common.h"

#include <stdlib.h>

#include "plugout.h"

#include "midifile.h"

#define FILENAMESIZE 32

static FILE *file = NULL;

static long track_length;
static long track_length_offset;
static long mute[4] = {0, 0, 0, 0};

int note[4];

static cycles_t cycles_prev = 0;

int midi_file_is_closed() {
	return file == NULL;
}

void midi_update_mute(const struct gbs_channel_status status[]) {
	for (int chan = 0; chan < 4; chan++)
		mute[chan] = status[chan].mute;
}

static int midi_write_data(const void *data, unsigned int length) {
	if (fwrite(data, 1, length, file) != length)
		return 1;

	track_length += length;
	return 0;
}

static int midi_write_string(const char *string) {
	return midi_write_data(string, strlen(string));;
}

static int midi_write_u32(uint32_t value)
{
	uint8_t data[4];

	data[0] = value >> 24;
	data[1] = value >> 16;
	data[2] = value >> 8;
	data[3] = value;

	return midi_write_data(data, 4);
}

static int midi_write_u16(uint16_t value)
{
	uint8_t data[2];

	data[0] = value >> 8;
	data[1] = value;

	return midi_write_data(data, 2);
}

static int midi_write_varlen(unsigned long value)
{
	uint8_t data[4];
	unsigned int i;

	for (i = 0; i < 4; ++i) {
		data[3 - i] = (value & 0x7f) | 0x80;
		value >>= 7;

		if (!value) {
			++i;
			break;
		}
	}

	data[3] &= ~0x80;

	return midi_write_data(data + 4 - i, i);
}

static int midi_write_event(cycles_t cycles, const uint8_t *data, unsigned int length)
{
	unsigned long timestamp_delta = (cycles - cycles_prev) >> 14;
	if (midi_write_varlen(timestamp_delta))
		return 1;

	if (midi_write_data(data, length))
		return 1;

	cycles_prev = cycles;
	return 0;
}

static int midi_open_track(int subsong)
{
	char *filename;
	if ((filename = malloc(FILENAMESIZE)) == NULL)
		goto out;

	if (snprintf(filename, FILENAMESIZE, "gbsplay-%d.mid", subsong + 1) >= FILENAMESIZE)
		goto out;

	file = fopen(filename, "wb");
	if (!file) {
		free(filename);
		goto out;
	}

	free(filename);

	/* Header type */
	if (midi_write_string("MThd"))
		goto out;

	/* Header length */
	if (midi_write_u32(6))
		goto out;

	/* Format */
	if (midi_write_u16(0))
		goto out;

	/* Tracks */
	if (midi_write_u16(1))
		goto out;

	/* Division */
	/* TODO: Do some real calculation instead of this magic number :-) */
	if (midi_write_u16(124))
		goto out;

	/* Track type */
	if (midi_write_string("MTrk"))
		goto out;

	/* Track length (placeholder) */
	track_length = 0;
	track_length_offset = ftell(file);
	if (track_length_offset == -1)
		goto out;

	if (midi_write_u32(0))
		goto out;

	return 0;

out:
	if (file != NULL)
		fclose(file);
	file = NULL;
	return 1;
}

static int midi_close_track()
{
	uint8_t event[3];

	/* End of track */
	event[0] = 0xff;
	event[1] = 0x2f;
	event[2] = 0x00;

	if (midi_write_event(cycles_prev, event, 3))
		goto out;

	if (fseek(file, track_length_offset, SEEK_SET) == -1)
		goto out;

	if (midi_write_u32(track_length))
		goto out;

	fclose(file);
	file = NULL;
	return 0;

out:
	file = NULL;
	return 1;
}

int midi_note_on(cycles_t cycles, int channel, int new_note, int velocity)
{
	uint8_t event[3];

	if (mute[channel])
		return 0;

	event[0] = 0x90 | channel;
	event[1] = new_note;
	event[2] = velocity;

	if (midi_write_event(cycles, event, 3))
		return 1;

	note[channel] = new_note;

	return 0;
}

int midi_note_off(cycles_t cycles, int channel)
{
	uint8_t event[3];

	if (!note[channel])
		return 0;

	event[0] = 0x80 | channel;
	event[1] = note[channel];
	event[2] = 0;

	if (midi_write_event(cycles, event, 3))
		return 1;

	note[channel] = 0;

	return 0;
}

int midi_pan(cycles_t cycles, int channel, int pan)
{
	uint8_t event[3];

	if (mute[channel])
		return 0;

	event[0] = 0xb0 | channel;
	event[1] = 0x0a;
	event[2] = pan;

	if (midi_write_event(cycles, event, 3))
		return 1;

	return 0;
}

long midi_open(enum plugout_endian endian, long rate, long *buffer_bytes)
{
	UNUSED(endian);
	UNUSED(rate);
	UNUSED(buffer_bytes);

	return 0;
}

int midi_skip(int subsong)
{
	int channel;

	if (file) {
		if (midi_close_track())
			return 1;
	}

	cycles_prev = 0;

	for (channel = 0; channel < 4; channel++)
		note[channel] = 0;

	return midi_open_track(subsong);
}

void midi_close(void)
{
	int channel;

	if (!file)
		return;

	for (channel = 0; channel < 4; channel++)
		if (midi_note_off(cycles_prev + 1, channel))
			return;

	midi_close_track();
}

