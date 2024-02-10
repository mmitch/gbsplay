/*
 * gbsplay is a Gameboy sound player
 *
 * 2003-2024 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                  Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include "gblfsr.h"
#include "test.h"

/* our LFSR shifts left so we can save on some shifting and masking,
   hence the bit order is reversed */
#define TAP_0		(1 << 14)
#define TAP_1		(1 << 13)
#define FB_6		(1 << 8)
#define MASK_FULL	((1 << 15) - 1)
#define MASK_NARROW	(((1 << 7) - 1) << (15 - 7))

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

test void test_lsfr()
{
	struct gblfsr state;
	uint32_t xw, xn;
	int n;

	gblfsr_reset(&state);
	n = 0;
	do {
		gblfsr_next_value(&state);
		n++;
	} while ((state.lfsr & MASK_FULL) != 0 && n < 99999);
	ASSERT_EQUAL("%d", n, 32767);

	gblfsr_reset(&state);
	gblfsr_set_narrow(&state, true);
	n = 0;
	do {
		gblfsr_next_value(&state);
		n++;
	} while ((state.lfsr & MASK_NARROW) != 0 && n < 999);
	ASSERT_EQUAL("%d", n, 127);

	gblfsr_reset(&state);
	for (n = 0; n < 32; n++) {
		xw <<= 1;
		xw |= gblfsr_next_value(&state);
	}
	ASSERT_EQUAL("%08x", xw, 0xfffc0008);

	gblfsr_reset(&state);
	gblfsr_set_narrow(&state, true);
	for (n = 0; n < 32; n++) {
		xn <<= 1;
		xn |= gblfsr_next_value(&state);
	}
	ASSERT_EQUAL("%08x", xn, 0xfc0830a3);
}
TEST(test_lsfr);
TEST_EOF;
