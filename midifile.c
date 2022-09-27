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

#include <stdlib.h>

#include "common.h"
#include "filewriter.h"
#include "plugout.h"
#include "midifile.h"

static long track_length_offset;
static long track_start_offset;
static long mute[4] = {0, 0, 0, 0};
static FILE* file = NULL;
static cycles_t cycles_prev = 0;

int note[4];

int midi_file_is_closed() {
	return file == NULL;
}

static int midi_file_close() {
	int result;
	if (midi_file_is_closed())
		return -1;

	result = file_close(file);
	file = NULL;
	return result;
}

void midi_update_mute(const struct gbs_channel_status status[]) {
	for (int chan = 0; chan < 4; chan++)
		mute[chan] = status[chan].mute;
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

	return file_write_bytes(file, data + 4 - i, i);
}

static int midi_write_event(cycles_t cycles, const uint8_t *data, unsigned int length)
{
	cycles_t cycles_delta = cycles - cycles_prev;
	unsigned long timestamp_delta = (cycles_delta) >> 14;

	if (midi_write_varlen(timestamp_delta))
		return 1;

	if (file_write_bytes(file, data, length))
		return 1;

	// only advance as far as the timestamp resolution allows, so we don't
	// accumulate errors from repeatedly throwing away the lower bits
	cycles_prev += timestamp_delta << 14;

	return 0;
}

static int midi_open_track(int subsong)
{
	if ((file = file_open("mid", subsong)) == NULL)
		goto error;

	/* Header type */
	if (file_write_string(file, "MThd"))
		goto error;

	/* Header length */
	if (file_write_32bit_be(file, 6))
		goto error;

	/* Format */
	if (file_write_16bit_be(file, 0))
		goto error;

	/* Tracks */
	if (file_write_16bit_be(file, 1))
		goto error;

	/* Division */
	/* TODO: Do some real calculation instead of this magic number :-) */
	if (file_write_16bit_be(file, 124))
		goto error;

	/* Track type */
	if (file_write_string(file, "MTrk"))
		goto error;

	/* Track length (placeholder) */
	track_length_offset = file_write_32bit_placeholder(file);
	if (track_length_offset == -1)
		goto error;

	/* remember where we are to calculate the track length later */
	track_start_offset = file_get_position(file);
	if (track_start_offset == -1)
		goto error;

	return 0;

error:
	if (file != NULL)
		midi_file_close();
	return 1;
}

static int midi_close_track()
{
	long track_end_offset;
	long track_length;
	uint8_t event[3];

	/* End of track */
	event[0] = 0xff;
	event[1] = 0x2f;
	event[2] = 0x00;

	if (midi_write_event(cycles_prev, event, 3))
		goto error;

	/* Update length in header */
	track_end_offset = file_get_position(file);
	if (track_end_offset == -1)
		goto error;

	track_length = track_end_offset - track_start_offset;
	if (file_write_32bit_be_at(file, track_length, track_length_offset) == -1)
		goto error;

	/* Close the file */
	if (midi_file_close() == -1)
		goto error;

	return 0;

error:
	if (file != NULL)
		midi_file_close();
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

long midi_open(enum plugout_endian *endian, long rate, long *buffer_bytes)
{
	UNUSED(endian);
	UNUSED(rate);
	UNUSED(buffer_bytes);

	return 0;
}

int midi_skip(int subsong)
{
	int channel;

	if (!midi_file_is_closed()) {
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

	if (midi_file_is_closed())
		return;

	for (channel = 0; channel < 4; channel++)
		if (midi_note_off(cycles_prev + 1, channel))
			return;

	midi_close_track();
}

