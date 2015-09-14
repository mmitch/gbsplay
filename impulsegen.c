/*
 * gbsplay is a Gameboy sound player
 *
 * 2003-2006 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 * Licensed under GNU GPL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "impulsegen.h"
#include "test.h"

static double sinc(double x)
{
	double a = M_PI*x;

	if (a == 0.0) {
		/* Never reached. */
		fprintf(stderr, "sinc(0), should not happen\n");
		return 1.0;
	}

	return sin(a)/a;
}

static double blackman(double n, double m)
{
	return 0.42 - 0.5 * cos(2*n*M_PI/m) + 0.08 * cos(4*n*M_PI/m);
}

short *gen_impulsetab(long w_shift, long n_shift, double cutoff)
{
	long i, j_l;
	long width = 1 << w_shift;
	long n = 1 << n_shift;
	long m = width/2;
	long size = width*n*sizeof(short);
	short *pulsetab = malloc(size);
	short *ptr;

	if (!pulsetab)
		return NULL;

	memset(pulsetab, 0, size);

	/* special-case for j_l == 0 to avoid sinc(0). */
	pulsetab[m-1] = IMPULSE_HEIGHT;
	ptr = &pulsetab[width];

	for (j_l=1; j_l < n; j_l ++) {
		double j = (double)j_l / n;
		long sum=0.0;
		long corr;
		long n = 0;
		double div = IMPULSE_HEIGHT;
		double dcorr = cutoff;

		do {
			sum = 0;
			for (i=-m+1; i<=m; i++) {
				double xd = dcorr*IMPULSE_HEIGHT*sinc((i-j)*cutoff)*blackman(i-j+width/2,width);
				short x = rint(xd);
				sum += x;
			}
			corr = IMPULSE_HEIGHT - sum;
			dcorr *= 1.0 + corr / div;
			div *= 1.3;
		} while ((corr != 0) && (n++ < 20));

		sum = 0;
		for (i=-m+1; i<=m; i++) {
			double xd = dcorr*IMPULSE_HEIGHT*sinc((i-j)*cutoff)*blackman(i-j+width/2,width);
			short x = rint(xd);
			*(ptr++) = x;
			sum += x;
		}
		*(ptr - m) += IMPULSE_HEIGHT - sum;
	}

	return pulsetab;
}

test void test_gen_impulsetab()
{
	const long n_shift = 3;
	const long w_shift = 5;
	const double cutoff = 1.0;
	const short reference[(1 << 3) * (1 << 5)] = {
		0, 0, 0, 0,  0, 0,  0, 0,  0, 0,  0,  0,   0,  0,   0, 256,   0,   0,  0,   0,  0,  0, 0,  0, 0,  0, 0,  0, 0, 0, 0, 0,
		0, 0, 0, 0,  0, 1, -1, 1, -2, 3, -4,  6,  -9, 14, -27, 249,  35, -16, 10,  -6,  4, -3, 2, -1, 1, -1, 0,  0, 0, 0, 0, 0,
		0, 0, 0, 0, -1, 1, -1, 2, -3, 5, -7, 10, -15, 24, -45, 229,  76, -31, 19, -12,  8, -6, 4, -3, 2, -1, 1,  0, 0, 0, 0, 0,
		0, 0, 0, 0, -1, 1, -2, 3, -4, 6, -9, 13, -19, 29, -54, 202, 121, -45, 26, -17, 12, -8, 6, -4, 2, -2, 1, -1, 0, 0, 0, 0,
		0, 0, 0, 0, -1, 1, -2, 3, -4, 6, -9, 13, -19, 30, -52, 162, 162, -52, 30, -19, 13, -9, 6, -4, 3, -2, 1, -1, 0, 0, 0, 0,
		0, 0, 0, 0, -1, 1, -2, 2, -4, 6, -8, 12, -17, 26, -45, 121, 202, -54, 29, -19, 13, -9, 6, -4, 3, -2, 1, -1, 0, 0, 0, 0,
		0, 0, 0, 0,  0, 1, -1, 2, -3, 4, -6,  8, -12, 19, -31,  76, 229, -45, 24, -15, 10, -7, 5, -3, 2, -1, 1, -1, 0, 0, 0, 0,
		0, 0, 0, 0,  0, 0, -1, 1, -1, 2, -3,  4,  -6, 10, -16,  35, 249, -27, 14,  -9,  6, -4, 3, -2, 1, -1, 1,  0, 0, 0, 0, 0,
	};
	short *pulsetab = gen_impulsetab(w_shift, n_shift, cutoff);

	ASSERT_ARRAY_EQUAL("%d", reference, pulsetab);
}
TEST(test_gen_impulsetab);
TEST_EOF;
