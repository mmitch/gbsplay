/* $ID$
 *
 * gbsplay is a Gameboy sound player
 *
 * 2004 (C) by Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL.
 *
 * Header file for sound output plugin architecture.
 */

#ifndef _PLUGINS_H_
#define _PLUGINS_H_

#include "common.h"
#include <unistd.h>

typedef int     regparm (*plugout_open_fn )(int endian, int rate);
typedef ssize_t regparm (*plugout_write_fn)(int fd, const void *buf, size_t count);
typedef int     regparm (*plugout_close_fn)(int fd);

struct output_plugin {
	char *id;
	char *name;
	plugout_open_fn  open_fn;
	plugout_write_fn write_fn;
	plugout_close_fn close_fn;
};

#endif
