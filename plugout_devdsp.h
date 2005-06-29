/* $Id: plugout_devdsp.h,v 1.5 2005/06/29 00:34:58 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2004 (C) by Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL.
 *
 * header file for /dev/dsp sound output plugin
 */

#include "common.h"

#ifndef _PLUGOUT_DEVDSP_H_
#define _PLUGOUT_DEVDSP_H_

void    regparm devdsp_open (long endian, long rate);
ssize_t regparm devdsp_write(const void *buf, size_t count);
void    regparm devdsp_close();

#endif
