/* $Id: util.c,v 1.2 2005/06/29 00:34:58 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL.
 *
 * This file contains various toolbox functions that
 * can be useful in different parts of gbsplay.
 */

#include <stdlib.h>

inline long rand_long(long max)
/* return random long from [0;max[ */
{
	return (long) (((double)max)*rand()/(RAND_MAX+1.0));
}

void shuffle_long(long *array, long elements)
/* shuffle an int array in place
 * Fisher-Yates algorithm, see `perldoc -q shuffle` :-)  */
{
	long i, j, temp;
	for (i = elements-1; i > 0; i--) {
		j=rand_long(i);      /* pick element  */
		temp = array[i];     /* swap elements */
		array[i] = array[j];
		array[j] = temp;
	}
}
