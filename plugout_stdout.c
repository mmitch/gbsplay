/* $Id: plugout_stdout.c,v 1.8 2005/08/06 21:33:16 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2004 (C) by Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL.
 *
 * STDOUT file writer output plugin
 */

#include "common.h"

#include <unistd.h>

#include "plugout.h"

int fd;

static long regparm stdout_open(enum plugout_endian endian, long rate)
{
	/*
	 * clone and close STDOUT_FILENO
	 * to make sure nobody else can write to stdout
	 */
	fd = dup(STDOUT_FILENO);
	if (fd == -1) return -1;
	close(STDOUT_FILENO);

	return 0;
}

static ssize_t regparm stdout_write(const void *buf, size_t count)
{
	return write(fd, buf, count);
}

static void regparm stdout_close()
{
	close(fd);
}

const struct output_plugin plugout_stdout = {
	.name = "stdout",
	.description = "STDOUT file writer",
	.open = stdout_open,
	.write = stdout_write,
	.close = stdout_close,
	.flags = PLUGOUT_USES_STDOUT,
};
