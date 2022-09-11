/*
 * gbsplay is a Gameboy sound player
 *
 * This file contains various toolbox functions that
 * can be useful in different parts of gbsplay.
 *
 * 2003-2020 (C) by Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
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
/* shuffle a long array in place
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
	long actual[]   = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
#ifdef __MSYS__
	long expected[] = { 7, 8, 9, 2, 6, 3, 4, 5, 1 };
#else
	long expected[] = { 2, 8, 9, 1, 6, 4, 5, 3, 7 };
#endif
	long len = sizeof(actual) / sizeof(*actual);
	int i;

	srand(0);
	shuffle_long(actual, len);

	for (i=0; i<len; i++) {
		ASSERT_EQUAL("%ld", actual[i], expected[i]);
	}
}
TEST(test_shuffle);
TEST_EOF;
