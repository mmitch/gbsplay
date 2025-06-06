/*
 * gbsplay is a Gameboy sound player
 *
 * This file contains the player code common to both CLI and X11 frontends.
 *
 * 2003-2025 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                  Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#ifndef _PLAYER_H_
#define _PLAYER_H_

#include "common.h"
#include "libgbs.h"
#include "plugout.h"
#include "cfgparser.h"

#define MAXOCTAVE 9

/* global variables */
extern char *myname;
extern char *filename;

extern plugout_open_fn  sound_open;
extern plugout_skip_fn  sound_skip;
extern plugout_pause_fn sound_pause;
extern plugout_io_fn    sound_io;
extern plugout_step_fn  sound_step;
extern plugout_write_fn sound_write;
extern plugout_close_fn sound_close;

struct displaytime {
	long played_min, played_sec, total_min, total_sec;
};

long is_running();
long nextsubsong_cb(struct gbs *gbs, void *priv);
void play_next_subsong(struct gbs *gbs);
void play_prev_subsong(struct gbs *gbs);
long step_emulation(struct gbs *gbs);
void toggle_pause();
const char *get_pause_string();
const char *get_loopmode_string(const struct gbs_status *status);
void update_displaytime(struct displaytime *time, const struct gbs_status *status);
struct gbs *common_init(int argc, char **argv);
void common_cleanup(struct gbs *gbs);

#endif
