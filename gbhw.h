/* $Id: gbhw.h,v 1.15 2005/06/30 00:55:56 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003-2005 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 * Licensed under GNU GPL.
 */

#ifndef _GBHW_H_
#define _GBHW_H_

#include <inttypes.h>
#include "common.h"

#define GBHW_CLOCK 4194304

struct gbhw_buffer {
	int16_t *data;
	long pos;
	long len;
};

struct gbhw_channel {
	long mute;
	long master;
	long leftgate;
	long rightgate;
	long volume;
	long env_dir;
	long env_tc;
	long env_ctr;
	long sweep_dir;
	long sweep_tc;
	long sweep_ctr;
	long sweep_shift;
	long len;
	long len_enable;
	long div_tc;
	long div_ctr;
	long duty_tc;
	long duty_ctr;
};

extern struct gbhw_channel gbhw_ch[4];

typedef regparm void (*gbhw_callback_fn)(struct gbhw_buffer *buf, void *priv);

regparm void gbhw_setcallback(gbhw_callback_fn fn, void *priv);
regparm void gbhw_setrate(long rate);
regparm void gbhw_setbuffer(struct gbhw_buffer *buffer);
regparm void gbhw_init(uint8_t *rombuf, uint32_t size);
regparm void gbhw_pause(long new_pause);
regparm void gbhw_master_fade(long speed, long dstvol);
regparm void gbhw_getminmax(int16_t *lmin, int16_t *lmax, int16_t *rmin, int16_t *rmax);
regparm long gbhw_step(long time_to_work);

#endif
