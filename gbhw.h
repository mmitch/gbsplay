/* $Id: gbhw.h,v 1.2 2003/08/24 10:56:13 ranma Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 */

#ifndef _GBHW_H_
#define _GBHW_H_

struct gbhw_channel {
	int master;
	int leftgate;
	int rightgate;
	int volume;
	int env_dir;
	int env_speed_tc;
	int env_speed;
	int sweep_dir;
	int sweep_speed_tc;
	int sweep_speed;
	int sweep_shift;
	int len;
	int len_enable;
	int div_tc;
	int div;
	int duty_tc;
	int duty;
};

extern struct gbhw_channel gbhw_ch[4];

typedef void (*gbhw_callback_fn)(void *buf, int len, void *priv);

void gbhw_setcallback(gbhw_callback_fn fn, void *priv);
void gbhw_setrate(int rate);
void gbhw_init(void);
int gbhw_step(void);
char *gbhw_romalloc(int size);
void gbhw_romfree(void);

#endif
