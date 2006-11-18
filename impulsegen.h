/* $Id: impulsegen.h,v 1.1 2006/11/18 14:34:52 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003-2006 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 * Licensed under GNU GPL.
 */

#ifndef _IMPULSEGEN_H_
#define _IMPULSEGEN_H_

#define IMPULSE_HEIGHT 256.0

short *gen_impulsetab(long w_shift, long n_shift, double cutoff);

#endif /* _IMPULSEGEN_H_ */
