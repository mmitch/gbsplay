/* $Id: plugout_devdsp.h,v 1.3 2004/03/20 20:32:04 mitch Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2004 (C) by Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL.
 *
 * header file for /dev/dsp sound output plugin
 */

#ifndef _PLUGOUT_DEVDSP_H_
#define _PLUGOUT_DEVDSP_H_

void    regparm devdsp_open (int endian, int rate);
ssize_t regparm devdsp_write(const void *buf, size_t count);
void    regparm devdsp_close();

#endif
