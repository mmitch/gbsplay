/*
 * gbsplay is a Gameboy sound player
 *
 * 2015 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *
 * Licensed under GNU GPL.
 */

#include "common.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "plugout.h"

static FILE *file;
static long cycles_prev = 0;

static long regparm iodumper_open(enum plugout_endian endian, long rate)
{
	/*
	 * clone and close STDOUT_FILENO
	 * to make sure nobody else can write to stdout
	 */
	int fd = dup(STDOUT_FILENO);
	if (fd == -1) return -1;
	(void)close(STDOUT_FILENO);
	file = fdopen(fd, "w");

	return 0;
}

static int regparm iodumper_skip(int subsong)
{
	fprintf(file, "\nsubsong %d\n", subsong);
	fprintf(stderr, "dumping subsong %d\n", subsong);

	return 0;
}

static int regparm iodumper_io(long cycles, uint32_t addr, uint8_t val)
{
	long cycle_diff = cycles - cycles_prev;

	fprintf(file, "%08lx %04x=%02x\n", cycle_diff, addr, val);
	cycles_prev = cycles;

	return 0;
}

static void regparm iodumper_close(void)
{
	fflush(file);
	fclose(file);
}

const struct output_plugin plugout_iodumper = {
	.name = "iodumper",
	.description = "STDOUT io dumper",
	.open = iodumper_open,
	.skip = iodumper_skip,
	.io = iodumper_io,
	.close = iodumper_close,
	.flags = PLUGOUT_USES_STDOUT,
};
