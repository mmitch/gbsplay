/* $Id: gbhw.h,v 1.13 2004/10/23 21:12:49 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 */

#ifndef _GBHW_H_
#define _GBHW_H_

#include <inttypes.h>
#include "common.h"

#define GBHW_CLOCK 4194304

struct gbhw_buffer {
	int16_t *data;
	int pos;
	int len;
};

struct gbhw_channel {
	int mute;
	int master;
	int leftgate;
	int rightgate;
	int volume;
	int env_dir;
	int env_tc;
	int env_ctr;
	int sweep_dir;
	int sweep_tc;
	int sweep_ctr;
	int sweep_shift;
	int len;
	int len_enable;
	int div_tc;
	int div_ctr;
	int duty_tc;
	int duty_ctr;
};

extern struct gbhw_channel gbhw_ch[4];

typedef regparm void (*gbhw_callback_fn)(struct gbhw_buffer *buf, void *priv);

regparm void gbhw_setcallback(gbhw_callback_fn fn, void *priv);
regparm void gbhw_setrate(int rate);
regparm void gbhw_setbuffer(struct gbhw_buffer *buffer);
regparm void gbhw_init(uint8_t *rombuf, uint32_t size);
regparm void gbhw_pause(int new_pause);
regparm void gbhw_master_fade(int speed, int dstvol);
regparm void gbhw_getminmax(int16_t *lmin, int16_t *lmax, int16_t *rmin, int16_t *rmax);
regparm int gbhw_step(int time_to_work);

#endif
