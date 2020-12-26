/*
 * gbsplay is a Gameboy sound player
 *
 * 2003-2020 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                  Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#ifndef _GBS_H_
#define _GBS_H_

#include <inttypes.h>
#include "common.h"
#include "libgbs.h"
#include "gbhw.h"
#include "libgbs.h"

#define GBS_LEN_SHIFT	10
#define GBS_LEN_DIV	(1 << GBS_LEN_SHIFT)

void gbs_set_gbhw_io_callback(struct gbs *gbs, gbhw_iocallback_fn fn, void *priv); // FIXME: don't export gbhw
void gbs_set_gbhw_step_callback(struct gbs *gbs, gbhw_stepcallback_fn fn, void *priv); // FIXME: don't export gbhw

#endif
