/*
 * gbsplay is a Gameboy sound player
 *
 * STDOUT file writer output plugin
 *
 * 2004-2020 (C) by Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include "common.h"

#include <unistd.h>

#include "plugout.h"

int fd;

static long stdout_open(enum plugout_endian endian,
                        long rate, long *buffer_bytes)
{
	/*
	 * clone and close STDOUT_FILENO
	 * to make sure nobody else can write to stdout
	 */
	fd = dup(STDOUT_FILENO);
	if (fd == -1) return -1;
	(void)close(STDOUT_FILENO);

	return 0;
}

static ssize_t stdout_write(const void *buf, size_t count)
{
	return write(fd, buf, count);
}

static void stdout_close()
{
	(void)close(fd);
}

const struct output_plugin plugout_stdout = {
	.name = "stdout",
	.description = "STDOUT file writer",
	.open = stdout_open,
	.write = stdout_write,
	.close = stdout_close,
	.flags = PLUGOUT_USES_STDOUT,
};
