#include "gb.h"

#define BASE_FREQ (HZ / 32)
#define FREQ_VAL(x) (2048 - (BASE_FREQ / (x)))

static void delay(int millis)
{
	/*
	 * DIV increases 16384 times per second, so this should be close enough
	 * to one millisecond per iteration.
	 */
	while (millis--) {
		char tmp = DIV + 16;
		while (DIV < tmp);
	};
}

static void ch1_trigger(long val)
{
	NR13 = val & 0xff;
	NR14 = 0x80 | ((val >> 8) & 7);
}

void main(void)
{
	NR52 = 0x80;  /* Enable audio block */
	NR51 = 0xff;  /* Route all channels to both left and right outputs */
	NR50 = 0x77;  /* Main output level 7 */

	NR10 = 0x00;  /* Sweep off */
	NR11 = 0x80;  /* Max length, 50% duty */
	NR12 = 0xf0;  /* Volume 15 */

	while (1) {
		ch1_trigger(FREQ_VAL(440));
		delay(500);
		ch1_trigger(FREQ_VAL(220));
		delay(500);
	}
}
