/* $Id: gbhw.h,v 1.7 2003/10/27 21:52:29 ranma Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 */

#ifndef _GBHW_H_
#define _GBHW_H_

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

typedef void (*gbhw_callback_fn)(void *buf, int len, void *priv);

void gbhw_setcallback(gbhw_callback_fn fn, void *priv);
void gbhw_setrate(int rate);
void gbhw_init(unsigned char *rombuf, unsigned int size);
void gbhw_pause(int new_pause);
void gbhw_master_fade(int speed, int dstvol);
void gbhw_getminmax(int *lmin, int *lmax, int *rmin, int *rmax);
int gbhw_step(int time_to_work);

#endif
