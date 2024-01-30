/*
 * gbsplay is a Gameboy sound player
 *
 * 2024 (C) by Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include <pipewire/pipewire.h>

#include "common.h"
#include "plugout.h"

static long pipewire_open(enum plugout_endian *endian, long rate, long *buffer_bytes)
{
	pw_init(0, NULL);
	
        fprintf(stdout,
		"\n\n"
		" >> pipewire plugout START\n"
		" >> Compiled with libpipewire %s\n"
		"Linked with libpipewire %s\n"
		"\n\n",
		pw_get_headers_version(),
		pw_get_library_version());

	return 0;
}

static ssize_t pipewire_write(const void *buf, size_t count)
{
	return 0; // dummy implementation
}

static void pipewire_close()
{
	fprintf(stdout, "\n\n >> pipewire plugout END \n\n");
	pw_deinit();
}

const struct output_plugin plugout_pipewire = {
	.name = "pipewire",
	.description = "PIPEWIRE sound driver",
	.open = pipewire_open,
	.write = pipewire_write,
	.close = pipewire_close,
};
