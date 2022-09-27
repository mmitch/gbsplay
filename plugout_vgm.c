/*
 * gbsplay is a Gameboy sound player
 *
 * VGM output plugin
 * 2022 (C) by Maximilian Rehkopf <otakon@gmx.net>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include "common.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "filewriter.h"
#include "plugout.h"

#define VGM_FILE_VERSION          (0x161)
#define VGM_DATA_START            (0x84)
#define VGM_DMG_CLOCK             (0x400000)
#define VGM_WAITSAMPLES_MAX       (0xffff)
#define VGM_TICKS_PER_SECOND      (44100)

#define VGM_HDR_NUMSAMPLES        (0x18)
#define VGM_HDR_DATA_START        (0x34)
#define VGM_HDR_DMG_CLOCK         (0x80)

#define VGM_CMD_WAITSAMPLES       (0x61)
#define VGM_CMD_WAITSAMPLES_SHORT (0x70)
#define VGM_CMD_END               (0x66)
#define VGM_CMD_DMGWRITE          (0xb3)

#define VGM_DATA_START_REL        (VGM_DATA_START - VGM_HDR_DATA_START)

static FILE *vgmfile;
static double samples_total = 0;
static double samples_prev = 0;
static double sample_diff_acc = 0;

/* finalize VGM output file */
static void vgm_finalize(void) {
	size_t eof_offset;
	/* append a second of delay at the end (for sounds to finish) */
	fputc(VGM_CMD_WAITSAMPLES, vgmfile);
	file_write_16bit_le(vgmfile, 1 * VGM_TICKS_PER_SECOND);

	/* write end of data marker */
	fputc(VGM_CMD_END, vgmfile);

	/* fill in header entries */
	eof_offset = ftell(vgmfile) - 4;
	fseek(vgmfile, 0, SEEK_SET);
	file_write_string(vgmfile, "Vgm ");
	file_write_32bit_le(vgmfile, eof_offset);
	file_write_32bit_le(vgmfile, VGM_FILE_VERSION);
	file_write_32bit_le_at(vgmfile, VGM_DMG_CLOCK, VGM_HDR_DMG_CLOCK);
	file_write_32bit_le_at(vgmfile, VGM_DATA_START_REL, VGM_HDR_DATA_START);
	file_write_32bit_le_at(vgmfile, (uint32_t)samples_total, VGM_HDR_NUMSAMPLES);
}

static long vgm_open(enum plugout_endian *endian, long rate, long *buffer_bytes) {
	return 0;
}

static int vgm_open_file(int subsong) {
	vgmfile = file_open("vgm", subsong);

	if(vgmfile == NULL) {
		fprintf(stderr, "Can't open output file: %s\n", strerror(errno));
		return -1;
	}

	/* zero-pad header area */
	for(int i = 0; i < (VGM_DATA_START / 4); i++) {
		file_write_32bit_le(vgmfile, 0);
	}

	sample_diff_acc = 0;
	samples_prev = 0;
	return 0;
}

static int vgm_close_file(void) {
	int result;
	vgm_finalize();
	result = file_close(vgmfile);
	vgmfile = NULL;
	return result;
}

static int vgm_skip(int subsong) {
	if(vgmfile) {
		if(vgm_close_file()) {
			return 1;
		};
	}

	return vgm_open_file(subsong);
}

static int vgm_io(cycles_t cycles, uint32_t addr, uint8_t val) {
	double sample_diff;
	int vgm_sample_diff;
	uint8_t vgmreg;

	/* calculate fractional samples (VGM counts everything as 44100Hz samples) */
	samples_total = (double)cycles * (double)VGM_TICKS_PER_SECOND / (double)VGM_DMG_CLOCK;
	sample_diff = samples_total - samples_prev;

	/* accumulate fractional samples, use integer part as delay time in samples */
	sample_diff_acc += sample_diff;
	vgm_sample_diff = sample_diff_acc;

	/* subtract integer part, keeping fractional part for further accumulation */
	sample_diff_acc -= vgm_sample_diff;

	/* write calculated sample delay commands in chunks of <= 65535 samples.
	   use single-byte delay shortcut command on 1..16 sample delays. */
	while(vgm_sample_diff > 0) {
		if(vgm_sample_diff < 17) {
			fputc(VGM_CMD_WAITSAMPLES_SHORT | (vgm_sample_diff - 1), vgmfile);
		} else {
			fputc(VGM_CMD_WAITSAMPLES, vgmfile);
			file_write_16bit_le(vgmfile, ((vgm_sample_diff > VGM_WAITSAMPLES_MAX)
			                               ? VGM_WAITSAMPLES_MAX
			                               : vgm_sample_diff));
		}
		vgm_sample_diff -= VGM_WAITSAMPLES_MAX;
	}

	/* dump the register write if register is valid for VGM dump. */
	if(addr >= 0xff10) {
		vgmreg = addr - 0xff10;

		fputc(VGM_CMD_DMGWRITE, vgmfile);
		fputc(vgmreg, vgmfile);
		fputc(val, vgmfile);
	}

	samples_prev = samples_total;

	return 0;
}

static void vgm_close(void) {
	if(vgmfile) {
		vgm_close_file();
	}
}

const struct output_plugin plugout_vgm = {
	.name = "vgm",
	.description = "VGM file writer",
	.open = vgm_open,
	.skip = vgm_skip,
	.io = vgm_io,
	.close = vgm_close
};
