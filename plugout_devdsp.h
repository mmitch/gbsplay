/* $ID$
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

int     regparm devdsp_open (int endian, int rate);
ssize_t regparm devdsp_write(int fd, const void *buf, size_t count);
int     regparm devdsp_close(int fd);

#endif
