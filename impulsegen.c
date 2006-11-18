/* $Id: impulsegen.c,v 1.1 2006/11/18 14:34:52 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003-2006 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 * Licensed under GNU GPL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "impulsegen.h"

static double sinc(double x)
{
	double a = M_PI*x;

	if (a == 0.0) return 1.0;

	return sin(a)/a;
}

static double blackman(double n, double m)
{
	return 0.42 - 0.5 * cos(2*n*M_PI/m) + 0.08 * cos(4*n*M_PI/m);
}

short *gen_impulsetab(long w_shift, long n_shift, double cutoff)
{
	long i;
	double j;
	long width = 1 << w_shift;
	long n = 1 << n_shift;
	long m = width/2;
	long size = width*n*sizeof(short);
	short *pulsetab = malloc(size);
	short *ptr = pulsetab;

	if (!pulsetab)
		return NULL;

	for (j=0; j<1.0; j+=1.0/n) {
		long sum=0;
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
