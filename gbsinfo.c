/* $Id: gbsinfo.c,v 1.2 2003/10/24 00:25:55 ranma Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include "gbhw.h"
#include "gbcpu.h"
#include "gbs.h"

int main(int argc, char **argv)
{
	struct gbs *gbs;

	if (argc != 2) {
		printf("Usage: %s <gbs-file>\n", argv[0]);
		exit(1);
	}

	if ((gbs = gbs_open(argv[1])) == NULL) exit(1);
	gbs_printinfo(gbs, 1);
	gbs_close(gbs);

	return 0;
}
