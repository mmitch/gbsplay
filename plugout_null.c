/*
 * gbsplay is a Gameboy sound player
 *
 * null sound output plugin
 *
 * 2026 (C) by Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include "common.h"
#include "plugout.h"

static long null_open(struct plugout_cfg *actual, long *buffer_bytes,
                        const struct plugout_metadata metadata)
{
	UNUSED(actual);
	UNUSED(buffer_bytes);
	UNUSED(metadata);

	return 0;
}

static ssize_t null_write(const void *buf, size_t count) {
	UNUSED(buf);
	UNUSED(count);
	return count;
}

static void null_close(void)
{
}

const struct output_plugin plugout_null = {
	.name = "null",
	.description = "NULL output",
	.open = null_open,
	.write = null_write,
	.close = null_close,
};
