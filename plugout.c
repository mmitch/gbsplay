/* $Id: plugout.c,v 1.7 2008/07/11 20:12:25 mitch Exp $
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
extern const struct output_plugin plugout_devdsp;
#endif
#ifdef PLUGOUT_ALSA
extern const struct output_plugin plugout_alsa;
#endif
#ifdef PLUGOUT_NAS
extern const struct output_plugin plugout_nas;
#endif
#ifdef PLUGOUT_STDOUT
extern const struct output_plugin plugout_stdout;
#endif
#ifdef PLUGOUT_MIDI
extern const struct output_plugin plugout_midi;
#endif

typedef /*@null@*/ const struct output_plugin* output_plugin_const_t;

static output_plugin_const_t plugouts[] = {
#ifdef PLUGOUT_NAS
	&plugout_nas,
#endif
#ifdef PLUGOUT_DEVDSP
	&plugout_devdsp,
#endif
#ifdef PLUGOUT_ALSA
	&plugout_alsa,
#endif
#ifdef PLUGOUT_STDOUT
	&plugout_stdout,
#endif
#ifdef PLUGOUT_MIDI
	&plugout_midi,
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
	(void)puts("");
}

regparm const struct output_plugin* plugout_select_by_name(const char *name)
{
	long idx;

	for (idx = 0; plugouts[idx] != NULL &&
	              strcmp(plugouts[idx]->name, name) != 0; idx++);

	return plugouts[idx];
}
