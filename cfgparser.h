/*
 * gbsplay is a Gameboy sound player
 *
 * 2003-2025 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                  Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#ifndef _CFGPARSER_H_
#define _CFGPARSER_H_

void  cfg_parse(const char* const fname);
char* get_userconfig(const char* const cfgfile);

#endif
