/*
 * gbsplay is a Gameboy sound player
 *
 * This file contains the interface to localized error messages.
 *
 * 2021 (C) by Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#ifndef _ERROR_H_
#define _ERROR_H_

void print_gbs_init_error(int error);
void print_gbs_open_error(int error, char* filename);

#endif
