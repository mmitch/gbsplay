/* $Id: plugout_stdout.h,v 1.2 2004/03/20 20:08:47 mitch Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2004 (C) by Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL.
 *
 * header file for STDOUT file writer output plugin
 */

#ifndef _PLUGOUT_STDOUT_H_
#define _PLUGOUT_STDOUT_H_

int     regparm stdout_open (int endian, int rate);
ssize_t regparm stdout_write(int fd, const void *buf, size_t count);
int     regparm stdout_close(int fd);

#endif
