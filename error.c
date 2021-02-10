/*
 * gbsplay is a Gameboy sound player
 *
 * This file contains localized error messages.
 *
 * 2003-2021 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                  Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include <stdlib.h>

#include "libgbs.h"

#include "error.h"

void print_gbs_open_error(int error, char *filename) {

	switch (error) {
	case -1:
		fprintf(stderr, _("Could not open %s: %s\n"), filename, strerror(errno));
		return;

	case -2:
		fprintf(stderr, _("Could not stat %s: %s\n"), filename, strerror(errno));
		return;

	case -3:
		fprintf(stderr, _("Could not read %s: %s\n"), filename, _("Bigger than allowed maximum (4MiB)"));
		return;

	case -4:
		fprintf(stderr, _("Could not read %s: %s\n"), filename, strerror(errno));
		return;

	case -5:
		fprintf(stderr, _("Not a GBS-File: %s\n"), filename);
		return;

	case -6:
		// fprintf(stderr, _("GBS Version %d unsupported.\n"), gbs->version); // FIXME: parameter missing
		fprintf(stderr, _("GBS version unsupported.\n")); // FIXME: parameter missing
		return;

	case -7:
		// fprintf(stderr, _("Number of subsongs = %d is unreasonable.\n"), gbs->songs); // FIXME: parameter missing
		fprintf(stderr, _("Number of subsongs is unreasonable.\n"));
		return;

	case -8:
		// fprintf(stderr, _("Default subsong %d is out of range [1..%d].\n"), gbs->defaultsong, gbs->songs); // FIXME: parameters missing
		fprintf(stderr, _("Default subsong %is out of range.\n"));
		return;

	case -9:
		fprintf(stderr, _("Could not open %s: %s\n"), filename, _("Not compiled with zlib support"));
		return;

	case -10:
		// fprintf(stderr, _("Could not open %s: inflateInit2: %d\n"), filename, ret); // FIXME: parameter missing
		fprintf(stderr, _("Could not open %s: inflateInit2\n"), filename);
		return;

	case -11:
		// fprintf(stderr, _("Could not open %s: inflate: %d\n"), filename, ret); // FIXME: parameter missing
		fprintf(stderr, _("Could not open %s: inflate\n"), filename);
		return;

	case -12:
		fprintf(stderr, _("Not a GBR-File: %s\n"), filename);
		return;

	case -13:
		// fprintf(stderr, _("Unsupported timerflag value: %d\n"), buf[0x07]); // FIXME: parameter missing
		fprintf(stderr, _("Unsupported timerflag value\n"));
		return;

	case -14:
		fprintf(stderr, _("Not a VGM-File: %s\n"), filename);
		return;

	case -15:
		// fprintf(stderr, _("Unsupported VGM version: %d.%02x\n"), buf[0x09], buf[0x08]); // FIXME: parameters missing
		fprintf(stderr, _("Unsupported VGM version\n"));
		return;

	case -16:
		// fprintf(stderr, _("Unsupported DMG clock: %ldHz\n"), dmg_clock); // FIXME: parameter missing
		fprintf(stderr, _("Unsupported DMG clock\n"));
		return;

	case -17:
		// fprintf(stderr, _("Bad file size in header: %ld\n"), eof_ofs); // FIXME: parameter missing
		fprintf(stderr, _("Bad file size in header\n"));
		return;

	case -18:
		// fprintf(stderr, _("Bad GD3 offset: %08lx\n"), gd3_ofs); // FIXME: parameter missing
		fprintf(stderr, _("Bad GD3 offset\n"));
		return;

	case -19:
		// fprintf(stderr, _("Bad data length: %ld\n"), data_len); // FIXME: parameter missing
		fprintf(stderr, _("Bad data length\n"));
		return;

	case -20:
		// fprintf(stderr, _("Unsupported VGM opcode: 0x%02x\n"), *data); // FIXME: parameter missing
		fprintf(stderr, _("Unsupported VGM opcode\n"));
		return;

	case -21:
		// fprintf(stderr, _("Unsupported cartridge type: 0x%02x\n"), buf[0x147]); // FIXME: parameter missing
		fprintf(stderr, _("Unsupported cartridge type\n"));
		return;

	default:
		fprintf(stderr, _("Could not read %s: unhandled gbs_open() error %d\n"), filename, error);
		return;
	}
}
