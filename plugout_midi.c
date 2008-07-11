/*
 * gbsplay is a Gameboy sound player
 *
 * 2006 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *
 * MIDI output plugin
 *
 * 2008 (C) by Vegard Nossum
 *
 * Licensed under GNU GPL.
 */

#include "common.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "plugout.h"

#define LN2 .69314718055994530941
#define MAGIC 5.78135971352465960412
#define FREQ(x) (262144 / (x))
#define NOTE(x) ((long)((log(FREQ(x))/LN2 - MAGIC)*12 + .2))

static long regparm midi_open(enum plugout_endian endian, long rate)
{
	return 0;
}

static FILE *file = NULL;

static long track_length;
static long track_length_offset;

static long cycles_prev = 0;

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
	do {
		uint8_t s;

		s = x & 0x7f;
		x >>= 7;
		if (x)
			s |= 0x80;

		if (fwrite(&s, 1, 1, file) != 1)
			return 1;

		++track_length;
	} while(x);

	return 0;
}

static int midi_write_event(long cycles, const uint8_t *s, unsigned int n)
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

	if (asprintf(&filename, "gbsplay-%d.mid", subsong + 1) == -1)
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

static int regparm midi_skip(int subsong)
{
	cycles_prev = 0;

	if (file) {
		if (midi_close_track())
			return 1;
	}

	return midi_open_track(subsong);
}

static int note_on(long cycles, int channel, int note, int velocity)
{
	uint8_t event[3];

	//event[0] = 0x90 | channel;
	event[0] = 0x90;
	event[1] = note;
	event[2] = velocity;

	if (midi_write_event(cycles, event, 3))
		return 1;

	return 0;
}

static int note_off(long cycles, int channel, int note)
{
	uint8_t event[3];

	//event[0] = 0x80 | channel;
	event[0] = 0x80;
	event[1] = note;
	event[2] = 0;

	if (midi_write_event(cycles, event, 3))
		return 1;

	return 0;
}

static int regparm midi_io(long cycles, uint32_t addr, uint8_t val)
{
	static long div[4] = {0, 0, 0, 0};
	static long note[4] = {0, 0, 0, 0};
	static int volume[4] = {0, 0, 0, 0};

	long chan = (addr - 0xff10) / 5;

	switch (addr) {
	case 0xff12:
	case 0xff17:
	case 0xff21:
		volume[chan] = 8 * (val >> 4);
		break;
	case 0xff13:
	case 0xff18:
	case 0xff1d:
		div[chan] &= 0xff00;
		div[chan] |= val;
		break;
	case 0xff14:
	case 0xff19:
	case 0xff1e:
		div[chan] &= 0x00ff;
		div[chan] |= ((long) (val & 7)) << 8;

		int new_note = NOTE(2048 - div[chan]) + 21;

		/* Channel start trigger */
		if (val & 0x80) {
			if (note[chan]) {
				if (note_off(cycles, chan, note[chan]))
					return 1;
			}

			if (new_note < 0 || new_note >= 0x80)
				break;

			if (note_on(cycles, chan, new_note, volume[chan]))
				return 1;
			note[chan] = new_note;
		} else {
			if (note_off(cycles, chan, note[chan]))
				return 1;
			note[chan] = 0;
		}

		break;
	case 0xff1a:
#if 0
		if (val) {
			if (note_on(cycles, 2, note[2], volume[2]))
				return 1;
		} else {
			if (note_off(cycles, 2, note[2]))
				return 1;
		}
#endif
		break;
	case 0xff1c:
		volume[2] = 32 * ((4 - ((val >> 5)) & 3) % 4);
		break;
	}

	return 0;
}

static ssize_t regparm midi_write(const void *buf, size_t count)
{
	return count;
}

static int regparm midi_close(void)
{
	if (file)
		return midi_close_track();

	return 0;
}

const struct output_plugin plugout_midi = {
	.name = "midi",
	.description = "MIDI sound driver",
	.open = midi_open,
	.skip = midi_skip,
	.io = midi_io,
	.write = midi_write,
	.close = midi_close,
};
