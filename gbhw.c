/* $Id: gbhw.c,v 1.3 2003/08/24 09:55:06 ranma Exp $
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

#include "gbcpu.h"
#include "gbhw.h"

static unsigned char *rom;
static unsigned char intram[0x2000];
static unsigned char extram[0x2000];
static unsigned char ioregs[0x80];
static unsigned char hiram[0x80];
static unsigned char rombank = 1;
static unsigned int romsize;
static unsigned int lastbank;

static char dutylookup[4] = {
	1, 2, 4, 6
};

struct gbhw_channel gbhw_ch[4];

static unsigned long long gb_clk;

static int vblanktc = 70256; /* ~59.7 Hz (vblank)*/
static int vblank = 70256;
static int timertc = 0;
static int timer = 0;

static gbhw_callback_fn callback;
static void *callbackpriv;

static unsigned char rom_get(unsigned short addr)
{
//	DPRINTF("rom_get(%04x)\n", addr);
	return rom[addr & 0x3fff];
}

static unsigned char rombank_get(unsigned short addr)
{
//	DPRINTF("rombank_get(%04x)\n", addr);
	return rom[(addr & 0x3fff) + 0x4000*rombank];
}

static unsigned char io_get(unsigned short addr)
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
	printf("ioread from 0x%04x unimplemented.\n", addr);
	DPRINTF("io_get(%04x)\n", addr);
	return 0xff;
}

static unsigned char intram_get(unsigned short addr)
{
//	DPRINTF("intram_get(%04x)\n", addr);
	return intram[addr & 0x1fff];
}

static unsigned char extram_get(unsigned short addr)
{
//	DPRINTF("extram_get(%04x)\n", addr);
	return extram[addr & 0x1fff];
}

static void rom_put(unsigned short addr, unsigned char val)
{
	if (addr >= 0x2000 && addr <= 0x3fff) {
		val &= 0x1f;
		rombank = val + (val == 0);
		if (rombank > lastbank) {
			printf("Bank %d out of range (0-%d)!\n", rombank, lastbank);
			rombank = lastbank;
		}
	}
}

static void io_put(unsigned short addr, unsigned char val)
{
	if (addr >= 0xff80 && addr <= 0xfffe) {
		hiram[addr & 0x7f] = val;
		return;
	} else if ((addr >= 0xff00 &&
	            addr <= 0xff7f) ||
	            addr == 0xffff) {
		ioregs[addr & 0x7f] = val;
		DPRINTF(" ([0x%04x]=%02x) ", addr, val);
		switch (addr) {
			case 0xff00:
				break;
			case 0xff06:
			case 0xff07:
				timertc = (256-ioregs[0x06]) * (16 << (((ioregs[0x07]+3) & 3) << 1));
				if ((ioregs[0x07] & 0xf0) == 0x80) timertc /= 2;
//				printf("Callback rate set to %2.2fHz.\n", 4194304/(float)timertc);
				break;
			case 0xff10:
				gbhw_ch[0].sweep_speed = gbhw_ch[0].sweep_speed_tc = ((val >> 4) & 7)*2;
				gbhw_ch[0].sweep_dir = (val >> 3) & 1;
				gbhw_ch[0].sweep_shift = val & 7;

				break;
			case 0xff11:
				{
					int duty = ioregs[0x11] >> 6;
					int len = ioregs[0x11] & 0x3f;

					gbhw_ch[0].duty = dutylookup[duty];
					gbhw_ch[0].duty_tc = gbhw_ch[0].div_tc*gbhw_ch[0].duty/8;
					gbhw_ch[0].len = (64 - len)*2;

					break;
				}
			case 0xff12:
				{
					int vol = ioregs[0x12] >> 4;
					int envdir = (ioregs[0x12] >> 3) & 1;
					int envspd = ioregs[0x12] & 7;

					gbhw_ch[0].volume = vol;
					gbhw_ch[0].env_dir = envdir;
					gbhw_ch[0].env_speed = gbhw_ch[0].env_speed_tc = envspd*8;
				}
				break;
			case 0xff13:
			case 0xff14:
				{
					int div = ioregs[0x13];

					div |= ((int)ioregs[0x14] & 7) << 8;
					gbhw_ch[0].div_tc = 2048 - div;
					gbhw_ch[0].duty_tc = gbhw_ch[0].div_tc*gbhw_ch[0].duty/8;

					if (addr == 0xff13) break;
				}
				if (val & 0x80) {
					int vol = ioregs[0x12] >> 4;
					int envdir = (ioregs[0x12] >> 3) & 1;
					int envspd = ioregs[0x12] & 7;
					int duty = ioregs[0x11] >> 6;
					int len = ioregs[0x11] & 0x3f;
					int div = ioregs[0x13];

					div |= ((int)val & 7) << 8;
					gbhw_ch[0].volume = vol;
					gbhw_ch[0].master = 1;
					gbhw_ch[0].env_dir = envdir;
					gbhw_ch[0].env_speed = gbhw_ch[0].env_speed_tc = envspd*8;

					gbhw_ch[0].div_tc = 2048 - div;
					gbhw_ch[0].duty = dutylookup[duty];
					gbhw_ch[0].duty_tc = gbhw_ch[0].div_tc*gbhw_ch[0].duty/8;
					gbhw_ch[0].len = (64 - len)*2;
					gbhw_ch[0].len_enable = (val & 0x40) > 0;

//					printf(" ch1: vol=%02d envd=%d envspd=%d duty=%d len=%02d len_en=%d key=%04d gate=%d%d\n", gbhw_ch[0].volume, gbhw_ch[0].env_dir, gbhw_ch[0].env_speed_tc, gbhw_ch[0].duty, gbhw_ch[0].len, gbhw_ch[0].len_enable, gbhw_ch[0].div_tc, gbhw_ch[0].leftgate, gbhw_ch[0].rightgate);
				}
				break;
			case 0xff15:
				break;
			case 0xff16:
				{
					int duty = ioregs[0x16] >> 6;
					int len = ioregs[0x16] & 0x3f;

					gbhw_ch[1].duty = dutylookup[duty];
					gbhw_ch[1].duty_tc = gbhw_ch[1].div_tc*gbhw_ch[1].duty/8;
					gbhw_ch[1].len = (64 - len)*2;

					break;
				}
			case 0xff17:
				{
					int vol = ioregs[0x17] >> 4;
					int envdir = (ioregs[0x17] >> 3) & 1;
					int envspd = ioregs[0x17] & 7;

					gbhw_ch[1].volume = vol;
					gbhw_ch[1].env_dir = envdir;
					gbhw_ch[1].env_speed = gbhw_ch[1].env_speed_tc = envspd*8;
				}
				break;
			case 0xff18:
			case 0xff19:
				{
					int div = ioregs[0x18];

					div |= ((int)ioregs[0x19] & 7) << 8;
					gbhw_ch[1].div_tc = 2048 - div;
					gbhw_ch[1].duty_tc = gbhw_ch[1].div_tc*gbhw_ch[1].duty/8;

					if (addr == 0xff18) break;
				}
				if (val & 0x80) {
					int vol = ioregs[0x17] >> 4;
					int envdir = (ioregs[0x17] >> 3) & 1;
					int envspd = ioregs[0x17] & 7;
					int duty = ioregs[0x16] >> 6;
					int len = ioregs[0x16] & 0x3f;
					int div = ioregs[0x18];

					div |= ((int)val & 7) << 8;
					gbhw_ch[1].volume = vol;
//					gbhw_ch[1].master = gbhw_ch[1].volume > 1; 
					gbhw_ch[1].master = 1;
					gbhw_ch[1].env_dir = envdir;
					gbhw_ch[1].env_speed = gbhw_ch[1].env_speed_tc = envspd*8;
					gbhw_ch[1].div_tc = 2048 - div;
					gbhw_ch[1].duty = dutylookup[duty];
					gbhw_ch[1].duty_tc = gbhw_ch[1].div_tc*gbhw_ch[1].duty/8;
					gbhw_ch[1].len = (64 - len)*2;
					gbhw_ch[1].len_enable = (val & 0x40) > 0;

//					printf(" ch2: vol=%02d envd=%d envspd=%d duty=%d len=%02d len_en=%d key=%04d gate=%d%d\n", gbhw_ch[1].volume, gbhw_ch[1].env_dir, gbhw_ch[1].env_speed, gbhw_ch[1].duty, gbhw_ch[1].len, gbhw_ch[1].len_enable, gbhw_ch[1].div_tc, gbhw_ch[1].leftgate, gbhw_ch[1].rightgate);
				}
				break;
			case 0xff1a:
				gbhw_ch[2].master = (ioregs[0x1a] & 0x80) > 0;
				break;
			case 0xff1b:
				{
					int len = ioregs[0x1b];

					gbhw_ch[2].len = (256 - len)*2;

					break;
				}
			case 0xff1c:
				{
					int vol = (ioregs[0x1c] >> 5) & 3;
					gbhw_ch[2].volume = vol;
					break;
				}
			case 0xff1d:
			case 0xff1e:
				{
					int div = ioregs[0x1d];
					div |= ((int)ioregs[0x1e] & 7) << 8;
					gbhw_ch[2].div_tc = 2048 - div;
					if (addr == 0xff1d) break;
				}
				if (val & 0x80) {
					int vol = (ioregs[0x1c] >> 5) & 3;
					int div = ioregs[0x1d];
					div |= ((int)val & 7) << 8;
					gbhw_ch[2].master = (ioregs[0x1a] & 0x80) > 0;
					gbhw_ch[2].volume = vol;
					gbhw_ch[2].div_tc = 2048 - div;
					gbhw_ch[2].len_enable = (val & 0x40) > 0;
//					printf(" ch3: sft=%02d envd=%d envspd=%d duty=%d len=%02d len_en=%d key=%04d gate=%d%d\n", gbhw_ch[2].volume, gbhw_ch[2].env_dir, gbhw_ch[2].env_speed, gbhw_ch[2].duty, gbhw_ch[2].len, gbhw_ch[2].len_enable, gbhw_ch[2].div_tc, gbhw_ch[2].leftgate, gbhw_ch[2].rightgate);
				}
				break;
			case 0xff1f:
				break;
			case 0xff20:
				{
					int len = ioregs[0x20] & 0x3f;

					gbhw_ch[3].len = (64 - len)*2;

					break;
				}
			case 0xff21:
				{
					int vol = ioregs[0x21] >> 4;
					int envdir = (ioregs[0x21] >> 3) & 1;
					int envspd = ioregs[0x21] & 7;

					gbhw_ch[3].volume = vol;
					gbhw_ch[3].env_dir = envdir;
					gbhw_ch[3].env_speed = gbhw_ch[3].env_speed_tc = envspd*8;
				}
				break;
			case 0xff22:
			case 0xff23:
				{
					int div = ioregs[0x22];
					int shift = div >> 5;
					int rate = div & 7;
					gbhw_ch[3].div = 0;
					gbhw_ch[3].div_tc = 1 << shift;
					if (rate) gbhw_ch[3].div_tc *= rate;
					else gbhw_ch[3].div_tc /= 2;
					if (addr == 0xff22) break;
				}
				if (val & 0x80) {
					int vol = ioregs[0x21] >> 4;
					int envdir = (ioregs[0x21] >> 3) & 1;
					int envspd = ioregs[0x21] & 7;
					int len = ioregs[0x20] & 0x3f;

					gbhw_ch[3].volume = vol;
					gbhw_ch[3].master = 1;
					gbhw_ch[3].env_dir = envdir;
					gbhw_ch[3].env_speed = gbhw_ch[3].env_speed_tc = envspd*8;
					gbhw_ch[3].len = (64 - len)*2;
					gbhw_ch[3].len_enable = (val & 0x40) > 0;

//					printf(" ch4: vol=%02d envd=%d envspd=%d duty=%d len=%02d len_en=%d key=%04d gate=%d%d\n", gbhw_ch[3].volume, gbhw_ch[3].env_dir, gbhw_ch[3].env_speed, gbhw_ch[3].duty, gbhw_ch[3].len, gbhw_ch[3].len_enable, gbhw_ch[3].div_tc, gbhw_ch[3].leftgate, gbhw_ch[3].rightgate);
				}
				break;
			case 0xff24:
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
			case 0xff27:
			case 0xff28:
			case 0xff29:
			case 0xff2a:
			case 0xff2b:
			case 0xff2c:
			case 0xff2d:
			case 0xff2e:
			case 0xff2f:
				break;
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
				break;
			case 0xffff:
				break;
			default:
				printf("iowrite to 0x%04x unimplemented (val=%02x).\n", addr, val);
				break;
		}
		return;
	}
	DPRINTF("io_put(%04x, %02x)\n", addr, val);
}

static void intram_put(unsigned short addr, unsigned char val)
{
	intram[addr & 0x1fff] = val;
}

static void extram_put(unsigned short addr, unsigned char val)
{
	extram[addr & 0x1fff] = val;
}

static short soundbuf[4096];
static int soundbufpos;
static int sound_div_tc;
static int sound_div;
static int main_div_tc = 32;
static int main_div;
static int sweep_div_tc = 256;
static int sweep_div;

int r_smpl;
int l_smpl;
int smpldivisor;

static unsigned int lfsr = 0xffffffff;
static int ch3pos;

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
		soundbuf[soundbufpos++] = l_smpl;
		soundbuf[soundbufpos++] = r_smpl;
		smpldivisor = 0;
		l_smpl = 0;
		r_smpl = 0;
		if (soundbufpos >= 4096) {
			soundbufpos = 0;
			callback(soundbuf, sizeof(soundbuf), callbackpriv);
		}
	}
	if (gbhw_ch[2].master) for (i=0; i<cycles; i++) {
		gbhw_ch[2].div--;
		if (gbhw_ch[2].div <= 0) {
			gbhw_ch[2].div = gbhw_ch[2].div_tc;
			ch3pos++;
		}
	}
	main_div += cycles;
	while (main_div > main_div_tc) {
		main_div -= main_div_tc;

		if (gbhw_ch[0].master) {
			int val = gbhw_ch[0].volume;
			if (gbhw_ch[0].div > gbhw_ch[0].duty_tc) {
				val = -val;
			}
			if (gbhw_ch[0].leftgate) l_smpl += val;
			if (gbhw_ch[0].rightgate) r_smpl += val;
			gbhw_ch[0].div--;
			if (gbhw_ch[0].div <= 0) {
				gbhw_ch[0].div = gbhw_ch[0].div_tc;
			}
		}
		if (gbhw_ch[1].master) {
			int val = gbhw_ch[1].volume;
			if (gbhw_ch[1].div > gbhw_ch[1].duty_tc) {
				val = -val;
			}
			if (gbhw_ch[1].leftgate) l_smpl += val;
			if (gbhw_ch[1].rightgate) r_smpl += val;
			gbhw_ch[1].div--;
			if (gbhw_ch[1].div <= 0) {
				gbhw_ch[1].div = gbhw_ch[1].div_tc;
			}
		}
		if (gbhw_ch[2].master) {
			int val = ((ioregs[0x30 + ((ch3pos >> 2) & 0xf)] >> ((~ch3pos & 2)*2)) & 0xf)*2;
			if (gbhw_ch[2].volume) {
				val = val >> (gbhw_ch[2].volume-1);
			} else val = 0;
//			val = val * gbhw_ch[2].volume/15;
			if (gbhw_ch[2].leftgate) l_smpl += val;
			if (gbhw_ch[2].rightgate) r_smpl += val;
		}
		if (gbhw_ch[3].master) {
//			int val = gbhw_ch[3].volume * (((lfsr >> 13) & 2)-1);
//			int val = gbhw_ch[3].volume * ((random() & 2)-1);
			static int val;
			if (gbhw_ch[3].leftgate) l_smpl += val;
			if (gbhw_ch[3].rightgate) r_smpl += val;
			gbhw_ch[3].div--;
			if (gbhw_ch[3].div <= 0) {
				gbhw_ch[3].div = gbhw_ch[3].div_tc;
				lfsr = (lfsr << 1) | (((lfsr >> 15) ^ (lfsr >> 14)) & 1);
				val = gbhw_ch[3].volume * ((random() & 2)-1);
//				val = gbhw_ch[3].volume * ((lfsr & 2)-1);
			}
		}
		smpldivisor++;

		sweep_div += 1;
		if (sweep_div >= sweep_div_tc) {
			sweep_div = 0;
			if (gbhw_ch[0].sweep_speed_tc) {
				gbhw_ch[0].sweep_speed--;
				if (gbhw_ch[0].sweep_speed < 0) {
					int val = gbhw_ch[0].div_tc >> gbhw_ch[0].sweep_shift;

					gbhw_ch[0].sweep_speed = gbhw_ch[0].sweep_speed_tc;
					if (gbhw_ch[0].sweep_dir) {
						if (gbhw_ch[0].div_tc < 2048 - val) gbhw_ch[0].div_tc += val;
					} else {
						if (gbhw_ch[0].div_tc > val) gbhw_ch[0].div_tc -= val;
					}
					gbhw_ch[0].duty_tc = gbhw_ch[0].div_tc*gbhw_ch[0].duty/8;
				}
			}
			if (gbhw_ch[0].len > 0 && gbhw_ch[0].len_enable) {
				gbhw_ch[0].len--;
				if (gbhw_ch[0].len == 0) gbhw_ch[0].volume = 0;
			}
			if (gbhw_ch[0].env_speed_tc) {
				gbhw_ch[0].env_speed--;
				if (gbhw_ch[0].env_speed <=0) {
					gbhw_ch[0].env_speed = gbhw_ch[0].env_speed_tc;
					if (!gbhw_ch[0].env_dir) {
						if (gbhw_ch[0].volume > 0)
							gbhw_ch[0].volume--;
					} else {
						if (gbhw_ch[0].volume < 15)
							gbhw_ch[0].volume++;
					}
				}
			}
			if (gbhw_ch[1].len > 0 && gbhw_ch[1].len_enable) {
				gbhw_ch[1].len--;
				if (gbhw_ch[1].len == 0) gbhw_ch[1].volume = 0;
			}
			if (gbhw_ch[1].env_speed_tc) {
				gbhw_ch[1].env_speed--;
				if (gbhw_ch[1].env_speed <=0) {
					gbhw_ch[1].env_speed = gbhw_ch[1].env_speed_tc;
					if (!gbhw_ch[1].env_dir) {
						if (gbhw_ch[1].volume > 0)
							gbhw_ch[1].volume--;
					} else {
						if (gbhw_ch[1].volume < 15)
							gbhw_ch[1].volume++;
					}
				}
			}
			if (gbhw_ch[2].len > 0 && gbhw_ch[2].len_enable) {
				gbhw_ch[2].len--;
				if (gbhw_ch[2].len == 0) gbhw_ch[2].volume = 0;
			}
			if (gbhw_ch[2].env_speed_tc) {
				gbhw_ch[2].env_speed--;
				if (gbhw_ch[2].env_speed <=0) {
					gbhw_ch[2].env_speed = gbhw_ch[2].env_speed_tc;
					if (!gbhw_ch[2].env_dir) {
						if (gbhw_ch[2].volume > 0)
							gbhw_ch[2].volume--;
					} else {
						if (gbhw_ch[2].volume < 15)
							gbhw_ch[2].volume++;
					}
				}
			}
			if (gbhw_ch[3].len > 0 && gbhw_ch[3].len_enable) {
				gbhw_ch[3].len--;
				if (gbhw_ch[3].len == 0) gbhw_ch[3].volume = 0;
			}
			if (gbhw_ch[3].env_speed_tc) {
				gbhw_ch[3].env_speed--;
				if (gbhw_ch[3].env_speed <=0) {
					gbhw_ch[3].env_speed = gbhw_ch[3].env_speed_tc;
					if (!gbhw_ch[3].env_dir) {
						if (gbhw_ch[3].volume > 0)
							gbhw_ch[3].volume--;
					} else {
						if (gbhw_ch[3].volume < 15)
							gbhw_ch[3].volume++;
					}
				}
			}
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
	sound_div_tc = (long long)4194304*65536/rate;
}

char *gbhw_romalloc(int size)
{
	romsize = (size + 0x3fff) & ~0x3fff;
	lastbank = (romsize / 0x4000) - 1;

	rom = malloc(romsize);
	return rom;
}

void gbhw_romfree(void)
{
	free(rom);
	romsize = 0;
	lastbank = 0;
}

void gbhw_init(void)
{
	callback = NULL;
	callbackpriv = NULL;
	gbhw_ch[0].duty = 4;
	gbhw_ch[1].duty = 4;
	gbhw_ch[0].div_tc = gbhw_ch[1].div_tc = gbhw_ch[2].div_tc = gbhw_ch[3].div_tc= 1;
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

int gbhw_step(void)
{
	int cycles = gbcpu_step();

	gb_sound(cycles);
	gb_clk += cycles;
	if (vblank > 0) vblank -= cycles;
	if (vblank <= 0 && interrupts && (ioregs[0x7f] & 1)) {
		vblank += vblanktc;
		gbcpu_intr(0x40);
	}
	if (timer > 0) timer -= cycles;
	if (timer <= 0 && interrupts && (ioregs[0x7f] & 4)) {
		timer += timertc;
		gbcpu_intr(0x48);
	}

	return cycles;
}
