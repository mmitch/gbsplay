/* $Id: util.c,v 1.1 2003/09/19 19:35:17 mitch Exp $
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

inline int rand_int(int max)
/* return random integer from [0;max[ */
{
	return (int) (((double)max)*rand()/(RAND_MAX+1.0));
}

void shuffle_int(int *array, int elements)
/* shuffle an int array in place
 * Fisher-Yates algorithm, see `perldoc -q shuffle` :-)  */
{
	int i, j, temp;
	for (i = elements-1; i > 0; i--) {
		j=rand_int(i);       /* pick element  */
		temp = array[i];     /* swap elements */
		array[i] = array[j];
		array[j] = temp;
	}
}
