/* $Id: gbhw.c,v 1.19 2003/11/30 14:25:24 ranma Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#include "gbcpu.h"
#include "gbhw.h"

static uint8_t *rom;
static uint8_t intram[0x2000];
static uint8_t extram[0x2000];
static uint8_t ioregs[0x80];
static uint8_t hiram[0x80];
static int rombank = 1;
static int romsize;
static int lastbank;

static const char dutylookup[4] = {
	1, 2, 4, 6
};

struct gbhw_channel gbhw_ch[4];

static int lminval, lmaxval, rminval, rmaxval;

#define MASTER_VOL_MIN	0
#define MASTER_VOL_MAX	256*256
static int master_volume;
static int master_fade;
static int master_dstvol;

static const int vblanktc = 70256; /* ~59.7 Hz (vblankctr)*/
static int vblankctr = 70256;
static int timertc = 70256;
static int timerctr = 70256;

#define HW_CLOCK 4194304
static const int msec_cycles = HW_CLOCK/1000;

static int pause_output = 0;

static gbhw_callback_fn callback;
static void *callbackpriv;

static uint8_t rom_get(uint32_t addr)
{
//	DPRINTF("rom_get(%04x)\n", addr);
	return rom[addr & 0x3fff];
}

static uint8_t rombank_get(uint32_t addr)
{
//	DPRINTF("rombank_get(%04x)\n", addr);
	return rom[(addr & 0x3fff) + 0x4000*rombank];
}

static uint8_t io_get(uint32_t addr)
{
	if (addr >= 0xff80 && addr <= 0xfffe) {
		return hiram[addr & 0x7f];
	}
	if (addr >= 0xff10 &&
	           addr <= 0xff3f) {
		return ioregs[addr & 0x7f];
	}
	if (addr == 0xff00) return 0;
	if (addr == 0xffff) return ioregs[0x7f];
	fprintf(stderr, "ioread from 0x%04x unimplemented.\n", addr);
	DPRINTF("io_get(%04x)\n", addr);
	return 0xff;
}

static uint8_t intram_get(uint32_t addr)
{
//	DPRINTF("intram_get(%04x)\n", addr);
	return intram[addr & 0x1fff];
}

static uint8_t extram_get(uint32_t addr)
{
//	DPRINTF("extram_get(%04x)\n", addr);
	return extram[addr & 0x1fff];
}

static void rom_put(uint32_t addr, uint8_t val)
{
	if (addr >= 0x2000 && addr <= 0x3fff) {
		val &= 0x1f;
		rombank = val + (val == 0);
		if (rombank > lastbank) {
			fprintf(stderr, "Bank %d out of range (0-%d)!\n", rombank, lastbank);
			rombank = lastbank;
		}
	}
}

static void io_put(uint32_t addr, uint8_t val)
{
	int chn = (addr - 0xff10)/5;
	if (addr >= 0xff80 && addr <= 0xfffe) {
		hiram[addr & 0x7f] = val;
		return;
	}
	ioregs[addr & 0x7f] = val;
	DPRINTF(" ([0x%04x]=%02x) ", addr, val);
	switch (addr) {
		case 0xff06:
		case 0xff07:
			timertc = (256-ioregs[0x06]) * (16 << (((ioregs[0x07]+3) & 3) << 1));
			if ((ioregs[0x07] & 0xf0) == 0x80) timertc /= 2;
//			printf("Callback rate set to %2.2fHz.\n", HW_CLOCK/(float)timertc);
			break;
		case 0xff10:
			gbhw_ch[0].sweep_ctr = gbhw_ch[0].sweep_tc = ((val >> 4) & 7);
			gbhw_ch[0].sweep_dir = (val >> 3) & 1;
			gbhw_ch[0].sweep_shift = val & 7;

			break;
		case 0xff11:
		case 0xff16:
		case 0xff20:
			{
				int duty_ctr = val >> 6;
				int len = val & 0x3f;

				gbhw_ch[chn].duty_ctr = dutylookup[duty_ctr];
				gbhw_ch[chn].duty_tc = gbhw_ch[chn].div_tc*gbhw_ch[chn].duty_ctr/8;
				gbhw_ch[chn].len = (64 - len)*2;

				break;
			}
		case 0xff12:
		case 0xff17:
		case 0xff21:
			{
				int vol = val >> 4;
				int envdir = (val >> 3) & 1;
				int envspd = val & 7;

				gbhw_ch[chn].volume = vol;
				gbhw_ch[chn].env_dir = envdir;
				gbhw_ch[chn].env_ctr = gbhw_ch[chn].env_tc = envspd*8;
			}
			break;
		case 0xff13:
		case 0xff14:
		case 0xff18:
		case 0xff19:
		case 0xff1d:
		case 0xff1e:
			{
				int div = ioregs[0x13 + 5*chn];

				div |= ((int)ioregs[0x14 + 5*chn] & 7) << 8;
				gbhw_ch[chn].div_tc = 2048 - div;
				gbhw_ch[chn].duty_tc = gbhw_ch[chn].div_tc*gbhw_ch[chn].duty_ctr/8;

				if (addr == 0xff13 ||
				    addr == 0xff18 ||
				    addr == 0xff1e) break;
			}
			gbhw_ch[chn].len_enable = (ioregs[0x14 + 5*chn] & 0x40) > 0;

//			printf(" ch%d: vol=%02d envd=%d envspd=%d duty_ctr=%d len=%03d len_en=%d key=%04d gate=%d%d\n", chn, gbhw_ch[chn].volume, gbhw_ch[chn].env_dir, gbhw_ch[chn].env_tc, gbhw_ch[chn].duty_ctr, gbhw_ch[chn].len, gbhw_ch[chn].len_enable, gbhw_ch[chn].div_tc, gbhw_ch[chn].leftgate, gbhw_ch[chn].rightgate);
			break;
		case 0xff15:
			break;
		case 0xff1a:
			gbhw_ch[2].master = (ioregs[0x1a] & 0x80) > 0;
			break;
		case 0xff1b:
			gbhw_ch[2].len = (256 - val)*2;
			break;
		case 0xff1c:
			{
				int vol = (ioregs[0x1c] >> 5) & 3;
				gbhw_ch[2].volume = vol;
				break;
			}
		case 0xff1f:
			break;
		case 0xff22:
		case 0xff23:
			{
				int div = ioregs[0x22];
				int shift = div >> 5;
				int rate = div & 7;
				gbhw_ch[3].div_ctr = 0;
				gbhw_ch[3].div_tc = 1 << shift;
				if (rate) gbhw_ch[3].div_tc *= rate;
				else gbhw_ch[3].div_tc /= 2;
				gbhw_ch[3].div_tc *= 2;
				if (addr == 0xff22) break;
//				printf(" ch4: vol=%02d envd=%d envspd=%d duty_ctr=%d len=%03d len_en=%d key=%04d gate=%d%d\n", gbhw_ch[3].volume, gbhw_ch[3].env_dir, gbhw_ch[3].env_ctr, gbhw_ch[3].duty_ctr, gbhw_ch[3].len, gbhw_ch[3].len_enable, gbhw_ch[3].div_tc, gbhw_ch[3].leftgate, gbhw_ch[3].rightgate);
			}
			gbhw_ch[chn].len_enable = (val & 0x40) > 0;
			break;
		case 0xff25:
			gbhw_ch[0].leftgate = (val & 0x10) > 0;
			gbhw_ch[0].rightgate = (val & 0x01) > 0;
			gbhw_ch[1].leftgate = (val & 0x20) > 0;
			gbhw_ch[1].rightgate = (val & 0x02) > 0;
			gbhw_ch[2].leftgate = (val & 0x40) > 0;
			gbhw_ch[2].rightgate = (val & 0x04) > 0;
			gbhw_ch[3].leftgate = (val & 0x80) > 0;
			gbhw_ch[3].rightgate = (val & 0x08) > 0;
			break;
		case 0xff26:
			ioregs[0x26] = 0x80;
			break;
		case 0xff00:
		case 0xff24:
		case 0xff27:
		case 0xff28:
		case 0xff29:
		case 0xff2a:
		case 0xff2b:
		case 0xff2c:
		case 0xff2d:
		case 0xff2e:
		case 0xff2f:
		case 0xff30:
		case 0xff31:
		case 0xff32:
		case 0xff33:
		case 0xff34:
		case 0xff35:
		case 0xff36:
		case 0xff37:
		case 0xff38:
		case 0xff39:
		case 0xff3a:
		case 0xff3b:
		case 0xff3c:
		case 0xff3d:
		case 0xff3e:
		case 0xff3f:
		case 0xffff:
			break;
		default:
			fprintf(stderr, "iowrite to 0x%04x unimplemented (val=%02x).\n", addr, val);
			break;
	}
}

static void intram_put(uint32_t addr, uint8_t val)
{
	intram[addr & 0x1fff] = val;
}

static void extram_put(uint32_t addr, uint8_t val)
{
	extram[addr & 0x1fff] = val;
}

static short soundbuf[4096];
static int soundbufpos;
static int sound_div_tc;
static int sound_div;
static const int main_div_tc = 32;
static int main_div;
static const int sweep_div_tc = 256;
static int sweep_div;

int r_smpl;
int l_smpl;
int smpldivisor;

static uint32_t lfsr = 0xffffffff;
static int ch3pos;

static void gb_sound_sweep(void)
{
	int i;

	if (gbhw_ch[0].sweep_tc) {
		gbhw_ch[0].sweep_ctr--;
		if (gbhw_ch[0].sweep_ctr < 0) {
			int val = gbhw_ch[0].div_tc >> gbhw_ch[0].sweep_shift;

			gbhw_ch[0].sweep_ctr = gbhw_ch[0].sweep_tc;
			if (gbhw_ch[0].sweep_dir) {
				if (gbhw_ch[0].div_tc < 2048 - val) gbhw_ch[0].div_tc += val;
			} else {
				if (gbhw_ch[0].div_tc > val) gbhw_ch[0].div_tc -= val;
			}
			gbhw_ch[0].duty_tc = gbhw_ch[0].div_tc*gbhw_ch[0].duty_ctr/8;
		}
	}
	for (i=0; i<4; i++) {
		if (gbhw_ch[i].len > 0 && gbhw_ch[i].len_enable) {
			gbhw_ch[i].len--;
			if (gbhw_ch[i].len == 0) {
				gbhw_ch[i].volume = 0;
				gbhw_ch[i].env_tc = 0;
			}
		}
		if (gbhw_ch[i].env_tc) {
			gbhw_ch[i].env_ctr--;
			if (gbhw_ch[i].env_ctr <=0) {
				gbhw_ch[i].env_ctr = gbhw_ch[i].env_tc;
				if (!gbhw_ch[i].env_dir) {
					if (gbhw_ch[i].volume > 0)
						gbhw_ch[i].volume--;
				} else {
					if (gbhw_ch[i].volume < 15)
					gbhw_ch[i].volume++;
				}
			}
		}
	}
	if (master_fade) {
		master_volume += master_fade;
		if ((master_fade > 0 &&
		     master_volume >= master_dstvol) ||
		    (master_fade < 0 &&
		     master_volume <= master_dstvol)) {
			master_fade = 0;
			master_volume = master_dstvol;
		}
	}

}

void gbhw_master_fade(int speed, int dstvol)
{
	if (dstvol < MASTER_VOL_MIN) dstvol = MASTER_VOL_MIN;
	if (dstvol > MASTER_VOL_MAX) dstvol = MASTER_VOL_MAX;
	master_dstvol = dstvol;
	if (dstvol > master_volume)
		master_fade = speed;
	else master_fade = -speed;
}

static void gb_sound(int cycles)
{
	int i;
	sound_div += (cycles * 65536);
	while (sound_div > sound_div_tc) {
		sound_div -= sound_div_tc;
		r_smpl *= 256;
		l_smpl *= 256;
		r_smpl /= smpldivisor;
		l_smpl /= smpldivisor;
		r_smpl *= master_volume/256;
		l_smpl *= master_volume/256;
		r_smpl /= 256;
		l_smpl /= 256;
		soundbuf[soundbufpos++] = l_smpl;
		soundbuf[soundbufpos++] = r_smpl;
		if (l_smpl > lmaxval) lmaxval = l_smpl;
		if (l_smpl < lminval) lminval = l_smpl;
		if (r_smpl > rmaxval) rmaxval = r_smpl;
		if (r_smpl < rminval) rminval = r_smpl;
		smpldivisor = 0;
		l_smpl = 0;
		r_smpl = 0;
		if (soundbufpos >= 4096) {
			soundbufpos = 0;
			callback(soundbuf, sizeof(soundbuf), callbackpriv);
		}
	}
	if (gbhw_ch[2].master) for (i=0; i<cycles; i++) {
		gbhw_ch[2].div_ctr--;
		if (gbhw_ch[2].div_ctr <= 0) {
			gbhw_ch[2].div_ctr = gbhw_ch[2].div_tc;
			ch3pos++;
		}
	}
	main_div += cycles;
	while (main_div > main_div_tc) {
		main_div -= main_div_tc;

		for (i=0; i<2; i++) if (gbhw_ch[i].master) {
			int val = gbhw_ch[i].volume;
			if (gbhw_ch[i].div_ctr > gbhw_ch[i].duty_tc) {
				val = -val;
			}
			if (!gbhw_ch[i].mute) {
				if (gbhw_ch[i].leftgate) l_smpl += val;
				if (gbhw_ch[i].rightgate) r_smpl += val;
			}
			gbhw_ch[i].div_ctr--;
			if (gbhw_ch[i].div_ctr <= 0) {
				gbhw_ch[i].div_ctr = gbhw_ch[i].div_tc;
			}
		}
		if (gbhw_ch[2].master) {
			int val = ((ioregs[0x30 + ((ch3pos >> 2) & 0xf)] >> ((~ch3pos & 2)*2)) & 0xf)*2;
			if (gbhw_ch[2].volume) {
				val = val >> (gbhw_ch[2].volume-1);
			} else val = 0;
			if (!gbhw_ch[2].mute) {
				if (gbhw_ch[2].leftgate) l_smpl += val;
				if (gbhw_ch[2].rightgate) r_smpl += val;
			}
		}
		if (gbhw_ch[3].master) {
//			int val = gbhw_ch[3].volume * (((lfsr >> 13) & 2)-1);
//			int val = gbhw_ch[3].volume * ((random() & 2)-1);
			static int val;
			if (!gbhw_ch[3].mute) {
				if (gbhw_ch[3].leftgate) l_smpl += val;
				if (gbhw_ch[3].rightgate) r_smpl += val;
			}
			gbhw_ch[3].div_ctr--;
			if (gbhw_ch[3].div_ctr <= 0) {
				gbhw_ch[3].div_ctr = gbhw_ch[3].div_tc;
				lfsr = (lfsr << 1) | (((lfsr >> 15) ^ (lfsr >> 14)) & 1);
				val = gbhw_ch[3].volume * ((random() & 2)-1);
//				val = gbhw_ch[3].volume * ((lfsr & 2)-1);
			}
		}
		smpldivisor++;

		sweep_div += 1;
		if (sweep_div >= sweep_div_tc) {
			sweep_div = 0;
			gb_sound_sweep();
		}
	}
}

void gbhw_setcallback(gbhw_callback_fn fn, void *priv)
{
	callback = fn;
	callbackpriv = priv;
}

void gbhw_setrate(int rate)
{
	sound_div_tc = (long long)HW_CLOCK*65536/rate;
}

void gbhw_getminmax(int *lmin, int *lmax, int *rmin, int *rmax)
{
	*lmin = lminval;
	*lmax = lmaxval;
	*rmin = rminval;
	*rmax = rmaxval;
	lminval = rminval = INT_MAX;
	lmaxval = rmaxval = INT_MIN;
}

void gbhw_init(uint8_t *rombuf, uint32_t size)
{
	int i;

	rom = rombuf;
	romsize = (size + 0x3fff) & ~0x3fff;
	lastbank = (romsize / 0x4000) - 1;
	memset(gbhw_ch, 0, sizeof(gbhw_ch));
	master_volume = MASTER_VOL_MAX;
	master_fade = 0;
	soundbufpos = 0;
	lminval = rminval = INT_MAX;
	lmaxval = rmaxval = INT_MIN;
	for (i=0; i<4; i++) {
		gbhw_ch[i].duty_ctr = 4;
		gbhw_ch[i].div_tc = 1;
		gbhw_ch[i].master = 1;
		gbhw_ch[i].mute = 0;
	}
	memset(extram, 0, sizeof(extram));
	memset(intram, 0, sizeof(intram));
	memset(hiram, 0, sizeof(hiram));
	memset(ioregs, 0, sizeof(ioregs));
	gbcpu_init();
	gbcpu_addmem(0x00, 0x3f, rom_put, rom_get);
	gbcpu_addmem(0x40, 0x7f, rom_put, rombank_get);
	gbcpu_addmem(0xa0, 0xbf, extram_put, extram_get);
	gbcpu_addmem(0xc0, 0xfe, intram_put, intram_get);
	gbcpu_addmem(0xff, 0xff, io_put, io_get);
}

int gbhw_step(int time_to_work)
/*
 * Rückgabewert: Anzahl gelaufener CPU-Cycles
 */
{
	int cycles_total = 0;

	if (pause_output) {
		usleep(time_to_work*1000);
		return 0;
	}

	time_to_work *= msec_cycles;
	
	while (cycles_total < time_to_work) {
		int cycles = gbcpu_step();

		if (cycles < 0) return cycles;
		
		gb_sound(cycles);
		if (vblankctr > 0) vblankctr -= cycles;
		if (vblankctr <= 0 && gbcpu_if && (ioregs[0x7f] & 1)) {
			vblankctr += vblanktc;
			gbcpu_intr(0x40);
		}
		if (timerctr > 0) timerctr -= cycles;
		if (timerctr <= 0 && gbcpu_if && (ioregs[0x7f] & 4)) {
			timerctr += timertc;
			gbcpu_intr(0x48);
		}
		cycles_total += cycles;
	}

	return cycles_total;
}

void gbhw_pause(int new_pause)
{
	pause_output = new_pause != 0;
}
