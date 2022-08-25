/*
 * gbsplay is a Gameboy sound player
 *
 * MIDI output plugin
 *
 * 2008-2022 (C) by Vegard Nossum
 *                  Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include "common.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "plugout.h"

#define FILENAMESIZE 32

static long midi_open(enum plugout_endian endian, long rate, long *buffer_bytes)
{
	UNUSED(endian);
	UNUSED(rate);
	UNUSED(buffer_bytes);

	return 0;
}

static FILE *file = NULL;

static long track_length;
static long track_length_offset;

static cycles_t cycles_prev = 0;

static int note[4] = {0, 0, 0, 0};
static long mute[4] = {0, 0, 0, 0};

static int midi_write_u32(uint32_t x)
{
	uint8_t s[4];

	s[0] = x >> 24;
	s[1] = x >> 16;
	s[2] = x >> 8;
	s[3] = x;

	if (fwrite(s, 1, 4, file) != 4)
		return 1;
	return 0;
}

static int midi_write_u16(uint16_t x)
{
	uint8_t s[2];

	s[0] = x >> 8;
	s[1] = x;

	if (fwrite(s, 1, 2, file) != 2)
		return 1;
	return 0;
}

static int midi_write_varlen(unsigned long x)
{
	uint8_t data[4];
	unsigned int i;

	for (i = 0; i < 4; ++i) {
		data[3 - i] = (x & 0x7f) | 0x80;
		x >>= 7;

		if (!x) {
			++i;
			break;
		}
	}

	data[3] &= ~0x80;
	if (fwrite(data + 4 - i, 1, i, file) != i)
		return 1;

	track_length += i;
	return 0;
}

static int midi_write_event(cycles_t cycles, const uint8_t *s, unsigned int n)
{
	if (midi_write_varlen((cycles - cycles_prev) >> 14))
		return 1;

	if (fwrite(s, 1, n, file) != n)
		return 1;

	track_length += n;

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
	if (fwrite("MThd", 1, 4, file) != 4)
		goto out_file;

	/* Header length */
	if (midi_write_u32(6))
		goto out_file;

	/* Format */
	if (midi_write_u16(0))
		goto out_file;

	/* Tracks */
	if (midi_write_u16(1))
		goto out_file;

	/* Division */
	/* XXX: Do some real calculation instead of this magic number :-) */
	if (midi_write_u16(124))
		goto out_file;

	/* Track type */
	if (fwrite("MTrk", 1, 4, file) != 4)
		goto out_file;

	/* Track length (placeholder) */
	track_length = 0;
	track_length_offset = ftell(file);
	if (track_length_offset == -1)
		goto out_file;

	if (midi_write_u32(0))
		goto out_file;

	return 0;

out_file:
	fclose(file);
	file = NULL;
out:
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

static int midi_skip(int subsong)
{
	cycles_prev = 0;

	if (file) {
		if (midi_close_track())
			return 1;
	}

	return midi_open_track(subsong);
}

static int note_on(cycles_t cycles, int channel, int new_note, int velocity)
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

static int note_off(cycles_t cycles, int channel)
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

static int pan(cycles_t cycles, int channel, int pan)
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

static int midi_io(cycles_t cycles, uint32_t addr, uint8_t val)
{
	static long div[4] = {0, 0, 0, 0};
	static int volume[4] = {0, 0, 0, 0};
	static int running[4] = {0, 0, 0, 0};
	static int master[4] = {0, 0, 0, 0};
	int new_note;

	long chan = (addr - 0xff10) / 5;

	if (!file)
		return 1;

	switch (addr) {
	case 0xff12:
	case 0xff17:
		volume[chan] = 8 * (val >> 4);
		master[chan] = (val & 0xf8) != 0;
		if (!master[chan] && running[chan]) {
			/* DAC turned off, disable channel */
			if (note_off(cycles, chan))
				return 1;
			running[chan] = 0;
		}
		if (volume[chan]) {
			/* volume set to >0, restart current note */
			if (running[chan] && !note[chan]) {
				new_note = NOTE(2048 - div[chan]) + 21;
				if (new_note < 0 || new_note >= 0x80)
					break;
				if (note_on(cycles, chan, new_note, volume[chan]))
					return 1;
			}
		} else {
			/* volume set to 0, stop note (if any) */
			if (note_off(cycles, chan))
				return 1;
		}
		break;
	case 0xff13:
	case 0xff18:
	case 0xff1d:
		div[chan] &= 0xff00;
		div[chan] |= val;

		if (running[chan]) {
			new_note = NOTE(2048 - div[chan]) + 21;

			if (new_note != note[chan]) {
				/* portamento: retrigger with new note */
				if (note_off(cycles, chan))
					return 1;

				if (new_note < 0 || new_note >= 0x80)
					break;

				if (note_on(cycles, chan, new_note, volume[chan]))
					return 1;
			}
		}

		break;
	case 0xff14:
	case 0xff19:
	case 0xff1e:
		div[chan] &= 0x00ff;
		div[chan] |= ((long) (val & 7)) << 8;

		new_note = NOTE(2048 - div[chan]) + 21;

		/* Channel start trigger */
		if ((val & 0x80) == 0x80) {
			if (note_off(cycles, chan))
				return 1;

			if (new_note < 0 || new_note >= 0x80)
				break;

			if (master[chan]) {
				if (note_on(cycles, chan, new_note, volume[chan]))
					return 1;
				running[chan] = 1;
			}
		} else {
			if (running[chan]) {
				if (new_note != note[chan]) {
					/* portamento: retrigger with new note */
					if (note_off(cycles, chan))
						return 1;

					if (new_note < 0 || new_note >= 0x80)
						break;

					if (note_on(cycles, chan, new_note, volume[chan]))
						return 1;
				}
			}
		}

		break;
	case 0xff1a:
		master[2] = (val & 0x80) == 0x80;
		if (!master[2] && running[2]) {
			/* DAC turned off, disable channel */
			if (note_off(cycles, 2))
				return 1;
			running[2] = 0;
		}
		break;
	case 0xff1c:
		volume[2] = 32 * ((4 - (val >> 5)) & 3);
		if (volume[2]) {
			/* volume set to >0, restart current note */
			if (running[2] && !note[2]) {
				new_note = NOTE(2048 - div[chan]) + 21;
				if (new_note < 0 || new_note >= 0x80)
					break;
				if (note_on(cycles, 2, new_note, volume[2]))
					return 1;
			}
		} else {
			/* volume set to 0, stop note (if any) */
			if (note_off(cycles, 2))
				return 1;
		}
		break;
	case 0xff25:
		for (chan = 0; chan < 4; chan++)
			switch ((val >> chan) & 0x11) {
			case 0x10:
				pan(cycles, chan, 0);
				break;
			case 0x01:
				pan(cycles, chan, 127);
				break;
			default:
				pan(cycles, chan, 64);
			}
		break;
	case 0xff26:
		if ((val & 0x80) == 0) {
			for (chan = 0; chan < 4; chan++) {
				div[chan] = 0;
				volume[chan] = 0;
				running[chan] = 0;
				master[chan] = 1;
				if (note_off(cycles, 2))
					return 1;
			}
		}
		break;
	}

	return 0;
}

static int midi_step(cycles_t cycles, const struct gbs_channel_status status[]) {
	int chan;

	UNUSED(cycles);

	for (chan = 0; chan < 4; chan++)
		mute[chan] = status[chan].mute;

	return 0;
}

static void midi_close(void)
{
	int chan;
	
	if (!file)
		return;

	for (chan = 0; chan < 4; chan++)
		if (note_off(cycles_prev + 1, chan))
			return;
	
	midi_close_track();
}

const struct output_plugin plugout_midi = {
	.name = "midi",
	.description = "MIDI file writer",
	.open = midi_open,
	.skip = midi_skip,
	.step = midi_step,
	.io = midi_io,
	.close = midi_close,
};
