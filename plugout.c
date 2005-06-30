/* $Id: plugout.c,v 1.3 2005/06/30 00:55:58 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003-2005 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                  Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL.
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plugout.h"

#ifdef PLUGOUT_DEVDSP
extern struct output_plugin plugout_devdsp;
#endif
#ifdef PLUGOUT_NAS
extern struct output_plugin plugout_nas;
#endif
#ifdef PLUGOUT_STDOUT
extern struct output_plugin plugout_stdout;
#endif

static struct output_plugin *plugouts[] = {
#ifdef PLUGOUT_NAS
	&plugout_nas,
#endif
#ifdef PLUGOUT_DEVDSP
	&plugout_devdsp,
#endif
#ifdef PLUGOUT_STDOUT
	&plugout_stdout,
#endif
	NULL
};

regparm void plugout_list_plugins(void)
{
	long idx;

	printf(_("Available output plugins:\n\n"));

	if (plugouts[0] == NULL) {
		printf(_("No output plugins available.\n\n"));
		return;
	}

	for (idx = 0; plugouts[idx] != NULL; idx++) {
		printf("%s\t- %s\n", plugouts[idx]->name, plugouts[idx]->description);
	}
	puts("");
}

regparm struct output_plugin* plugout_select_by_name(char *name)
{
	long idx;

	for (idx = 0; plugouts[idx] != NULL &&
	              strcmp(plugouts[idx]->name, name) != 0; idx++);

	return plugouts[idx];
}
