/*
 * gbsplay is a Gameboy sound player
 *
 * 2003-2024 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                  Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include "gblfsr.h"

/* our LFSR shifts left so we can save on some shifting and masking,
   hence the bit order is reversed */
#define TAP_0		(1 << 14)
#define TAP_1		(1 << 13)
#define FB_6		(1 << 8)

void gblfsr_reset(struct gblfsr* gblfsr) {
	gblfsr->lfsr = 0;
	gblfsr->narrow = false;
}

void gblfsr_trigger(struct gblfsr* gblfsr) {
	gblfsr->lfsr = 0;
}

void gblfsr_set_narrow(struct gblfsr* gblfsr, bool narrow) {
	gblfsr->narrow = narrow;
}

int gblfsr_next_value(struct gblfsr* gblfsr) {
	long tap_out;
	tap_out = !(((gblfsr->lfsr & TAP_0) > 0) ^ ((gblfsr->lfsr & TAP_1) > 0));
	gblfsr->lfsr = (gblfsr->lfsr << 1) | tap_out;
	if(gblfsr->narrow) {
		gblfsr->lfsr = (gblfsr->lfsr & ~FB_6) | ((tap_out) * FB_6);
	}
	return !(gblfsr->lfsr & TAP_0);
}
