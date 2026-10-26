/*
 * gbsplay is a Gameboy sound player
 *
 * STDOUT file writer output plugin
 *
 * 2004-2026 (C) by Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include <unistd.h>
#include <fcntl.h>

#include "common.h"
#include "plugout.h"

static int plugout_stdout_fd;

static int set_fd_to_binmode(void)
{
	// skip this on POSIX, as there are no text and binary modes
#ifdef HAVE_SETMODE
	if (setmode(plugout_stdout_fd, O_BINARY) == -1)
		return -1;
#endif
	return 0;
}

static long stdout_open(struct plugout_cfg *actual, long *buffer_bytes,
                        const struct plugout_metadata metadata)
{
	UNUSED(actual);
	UNUSED(buffer_bytes);
	UNUSED(metadata);

	/*
	 * clone and close STDOUT_FILENO
	 * to make sure nobody else can write to stdout
	 */
	plugout_stdout_fd = dup(STDOUT_FILENO);
	if (plugout_stdout_fd == -1) return -1;
	(void)close(STDOUT_FILENO);

	if (set_fd_to_binmode() == -1)
		return -1;

	return 0;
}

static ssize_t stdout_write(const void *buf, size_t count)
{
	return write(plugout_stdout_fd, buf, count);
}

static void stdout_close(void)
{
	(void)close(plugout_stdout_fd);
}

const struct output_plugin plugout_stdout = {
	.name = "stdout",
	.description = "STDOUT file writer",
	.open = stdout_open,
	.write = stdout_write,
	.close = stdout_close,
	.flags = PLUGOUT_USES_STDOUT,
};
