/* $Id: gbsinfo.c,v 1.4 2003/12/12 23:05:51 ranma Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "gbhw.h"
#include "gbcpu.h"
#include "gbs.h"

int main(int argc, char **argv)
{
	struct gbs *gbs;

	i18n_init();

	if (argc != 2) {
		printf(_("Usage: %s <gbs-file>\n"), argv[0]);
		exit(1);
	}

	if ((gbs = gbs_open(argv[1])) == NULL) exit(1);
	gbs_printinfo(gbs, 1);
	gbs_close(gbs);

	return 0;
}
