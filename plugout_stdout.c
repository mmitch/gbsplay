/* $Id: plugout_stdout.c,v 1.3 2004/03/20 20:32:04 mitch Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2004 (C) by Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL.
 *
 * STDOUT file writer output plugin
 */

#include "plugins.h"

void regparm stdout_open(int endian, int rate)
{
}

ssize_t regparm stdout_write(const void *buf, size_t count)
{
	return write(STDOUT_FILENO, buf, count);
}

void regparm stdout_close()
{
}
