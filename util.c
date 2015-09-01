/*
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL.
 *
 * This file contains various toolbox functions that
 * can be useful in different parts of gbsplay.
 */

#include <stdlib.h>
#include "util.h"
#include "test.h"

long rand_long(long max)
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

test void test_shuffle()
{
	long array[]          = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	long shuffled_array[] = { 2, 8, 9, 1, 6, 4, 5, 3, 7 };
	long len = sizeof(array) / sizeof(*array);
	int i;

	srand(0);
	shuffle_long(array, len);

	for (i=0; i<len; i++) {
		ASSERT_EQUAL("%ld", array[i], shuffled_array[i]);
	}
}
TEST(test_shuffle);
TEST_EOF;
