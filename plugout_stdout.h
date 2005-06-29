/* $Id: plugout_stdout.h,v 1.5 2005/06/29 00:34:58 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2004 (C) by Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL.
 *
 * header file for STDOUT file writer output plugin
 */

#include "common.h"

#ifndef _PLUGOUT_STDOUT_H_
#define _PLUGOUT_STDOUT_H_

void    regparm stdout_open (long endian, long rate);
ssize_t regparm stdout_write(const void *buf, size_t count);
void    regparm stdout_close();

#endif
