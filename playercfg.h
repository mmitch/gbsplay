/*
 * gbsplay is a Gameboy sound player
 *
 * 2003-2025 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                  Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#ifndef _PLAYERCFG_H_
#define _PLAYERCFG_H_

#include "libgbs.h"

#define CFG_FILTER_OFF "off"
#define CFG_FILTER_DMG "dmg"
#define CFG_FILTER_CGB "cgb"

enum play_mode {
	PLAY_MODE_LINEAR  = 1,
	PLAY_MODE_RANDOM  = 2,
	PLAY_MODE_SHUFFLE = 3,
};

enum plugout_endian {
	PLUGOUT_ENDIAN_BIG,
	PLUGOUT_ENDIAN_LITTLE,
	PLUGOUT_ENDIAN_AUTOSELECT,
};

struct player_cfg {
	long fadeout;
	char *filter_type;
	enum gbs_loop_mode loop_mode;
	char *output_filename;
	enum play_mode play_mode;
	long refresh_delay;
	long silence_timeout;
	char *sound_name;
	long subsong_gap;
	long subsong_timeout;
	long verbosity;

	// prepend with 'requested_' to signal possible override in struct plugout_cfg
	enum plugout_endian requested_endian;
	long requested_rate;
};

// this is a global variable, but for the config options this seems acceptable
// currently the commandline arguments are handled in player.c so write access is needed there as well
extern struct player_cfg cfg;

#endif
