/* $ID$
 *
 * gbsplay is a Gameboy sound player
 *
 * 2004 (C) by Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL.
 *
 * STDOUT file writer output plugin
 */

#include "plugins.h"

int regparm stdout_open(int endian, int rate)
{
	return STDOUT_FILENO;
}

ssize_t regparm stdout_write(int fd, const void *buf, size_t count)
{
	return write(fd, buf, count);
}

int regparm stdout_close(int fd)
{
	return close(fd);
}
