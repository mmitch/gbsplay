/* $Id: gbsplay.c,v 1.19 2003/08/23 22:36:07 ranma Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#if !BYTE_ORDER || !BIG_ENDIAN || !LITTLE_ENDIAN
#error endian defines missing
#endif

#define ZF	0x80
#define NF	0x40
#define HF	0x20
#define CF	0x10

#define BC 0
#define DE 1
#define HL 2
#define FA 3
#define SP 4
#define PC 5

#define DEBUG 0

#if DEBUG == 1
#define DPRINTF(...) printf(__VA_ARGS__)
#define DEB(x) x
#else

static inline void foo(void)
{
}

#define DPRINTF(...) foo()
#define DEB(x) foo()
#endif

#if BYTE_ORDER == LITTLE_ENDIAN

#define REGS16_R(r, i) (*((unsigned short*)&r.ri[i*2]))
#define REGS16_W(r, i, x) (*((unsigned short*)&r.ri[i*2])) = x
#define REGS8_R(r, i) (r.ri[i^1])
#define REGS8_W(r, i, x) (r.ri[i^1]) = x

static struct regs {
	union {
		unsigned char ri[12];
		struct {
			unsigned char c;
			unsigned char b;
			unsigned char e;
			unsigned char d;
			unsigned char l;
			unsigned char h;
			unsigned char a;
			unsigned char f;
			unsigned short sp;
			unsigned short pc;
		} rn;
	};
} regs;

#else

#define REGS16_R(r, i) (*((unsigned short*)&r.ri[i*2]))
#define REGS16_W(r, i, x) (*((unsigned short*)&r.ri[i*2])) = x
#define REGS8_R(r, i) (*((unsigned short*)&r.ri[i*2]))
#define REGS8_W(r, i, x) (*((unsigned short*)&r.ri[i*2])) = x

static struct regs {
	union {
		unsigned char ri[12];
		struct {
			unsigned char b;
			unsigned char c;
			unsigned char d;
			unsigned char e;
			unsigned char h;
			unsigned char l;
			unsigned char f;
			unsigned char a;
			unsigned short sp;
			unsigned short pc;
		} rn;
	};
} regs;

#endif

static struct regs oldregs;

struct opinfo;

typedef void (*ex_fn)(unsigned char op, struct opinfo *oi);
typedef void (*put_fn)(unsigned short addr, unsigned char val);
typedef unsigned char (*get_fn)(unsigned short addr);

struct opinfo {
#if DEBUG == 1
	char *name;
#endif
	ex_fn fn;
};

#if DEBUG == 1
static char regnames[12] = "BCDEHLFASPPC";
static char *regnamech16[6] = {
	"BC", "DE", "HL", "FA", "SP", "PC"
};
#endif

static unsigned char *rom;
static unsigned char ram[0x4000];
static unsigned char ioregs[0x80];
static unsigned char hiram[0x80];
static unsigned char rombank = 1;

static unsigned short gbs_base;
static unsigned short gbs_init;
static unsigned short gbs_play;
static unsigned short gbs_stack;
static unsigned int romsize;
static unsigned int lastbank;

struct channel {
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

static char dutylookup[4] = {
	1, 2, 4, 6
};

struct channel ch1;
struct channel ch2;
struct channel ch3;
struct channel ch4;

static unsigned int cycles;
static unsigned long long clock;
static int halted;
static int interrupts;
static int timertc = 70256; /* ~59.7 Hz (vblank)*/
static int timer = 70256;

static unsigned char vidram_get(unsigned short addr)
{
//	DPRINTF("vidram_get(%04x)\n", addr);
	return 0xff;
}

static unsigned char rom_get(unsigned short addr)
{
//	DPRINTF("rom_get(%04x)\n", addr);
	if (addr < 0x4000) {
		return rom[addr];
	} else {
		return rom[addr - 0x4000 + 0x4000*rombank];
	}
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
	return ram[0x2000 + (addr & 0x1fff)];
}

static unsigned char extram_get(unsigned short addr)
{
//	DPRINTF("extram_get(%04x)\n", addr);
	return ram[addr & 0x1fff];
}

static get_fn getlookup[256] = {
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&rom_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&vidram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&extram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&intram_get,
	&io_get,
	&io_get
};

static inline unsigned char mem_get(unsigned short addr)
{
	unsigned char res = getlookup[addr >> 8](addr);
	cycles+=4;
	return res;
}

static void vidram_put(unsigned short addr, unsigned char val)
{
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
				printf("Callback rate set to %2.2fHz (Custom).\n", 4194304/(float)timertc);
				break;
			case 0xff10:
				ch1.sweep_speed = ch1.sweep_speed_tc = ((val >> 4) & 7)*2;
				ch1.sweep_dir = (val >> 3) & 1;
				ch1.sweep_shift = val & 7;

				break;
			case 0xff11:
				{
					int duty = ioregs[0x11] >> 6;
					int len = ioregs[0x11] & 0x3f;

					ch1.duty = dutylookup[duty];
					ch1.duty_tc = ch1.div_tc*ch1.duty/8;
					ch1.len = (64 - len)*2;

					break;
				}
			case 0xff12:
				{
					int vol = ioregs[0x12] >> 4;
					int envdir = (ioregs[0x12] >> 3) & 1;
					int envspd = ioregs[0x12] & 7;

					ch1.volume = vol;
					ch1.env_dir = envdir;
					ch1.env_speed = ch1.env_speed_tc = envspd*8;
				}
				break;
			case 0xff13:
			case 0xff14:
				{
					int div = ioregs[0x13];

					div |= ((int)ioregs[0x14] & 7) << 8;
					ch1.div_tc = 2048 - div;
					ch1.duty_tc = ch1.div_tc*ch1.duty/8;

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
					ch1.volume = vol;
					ch1.master = 1;
					ch1.env_dir = envdir;
					ch1.env_speed = ch1.env_speed_tc = envspd*8;

					ch1.div_tc = 2048 - div;
					ch1.duty = dutylookup[duty];
					ch1.duty_tc = ch1.div_tc*ch1.duty/8;
					ch1.len = (64 - len)*2;
					ch1.len_enable = (val & 0x40) > 0;

//					printf(" ch1: vol=%02d envd=%d envspd=%d duty=%d len=%02d len_en=%d key=%04d gate=%d%d\n", ch1.volume, ch1.env_dir, ch1.env_speed_tc, ch1.duty, ch1.len, ch1.len_enable, ch1.div_tc, ch1.leftgate, ch1.rightgate);
				}
				break;
			case 0xff15:
				break;
			case 0xff16:
				{
					int duty = ioregs[0x16] >> 6;
					int len = ioregs[0x16] & 0x3f;

					ch2.duty = dutylookup[duty];
					ch2.duty_tc = ch2.div_tc*ch2.duty/8;
					ch2.len = (64 - len)*2;

					break;
				}
			case 0xff17:
				{
					int vol = ioregs[0x17] >> 4;
					int envdir = (ioregs[0x17] >> 3) & 1;
					int envspd = ioregs[0x17] & 7;

					ch2.volume = vol;
					ch2.env_dir = envdir;
					ch2.env_speed = ch2.env_speed_tc = envspd*8;
				}
				break;
			case 0xff18:
			case 0xff19:
				{
					int div = ioregs[0x18];

					div |= ((int)ioregs[0x19] & 7) << 8;
					ch2.div_tc = 2048 - div;
					ch2.duty_tc = ch2.div_tc*ch2.duty/8;

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
					ch2.volume = vol;
//					ch2.master = ch2.volume > 1; 
					ch2.master = 1;
					ch2.env_dir = envdir;
					ch2.env_speed = ch2.env_speed_tc = envspd*8;
					ch2.div_tc = 2048 - div;
					ch2.duty = dutylookup[duty];
					ch2.duty_tc = ch2.div_tc*ch2.duty/8;
					ch2.len = (64 - len)*2;
					ch2.len_enable = (val & 0x40) > 0;

//					printf(" ch2: vol=%02d envd=%d envspd=%d duty=%d len=%02d len_en=%d key=%04d gate=%d%d\n", ch2.volume, ch2.env_dir, ch2.env_speed, ch2.duty, ch2.len, ch2.len_enable, ch2.div_tc, ch2.leftgate, ch2.rightgate);
				}
				break;
			case 0xff1a:
				ch3.master = (ioregs[0x1a] & 0x80) > 0;
				break;
			case 0xff1b:
				{
					int len = ioregs[0x1b];

					ch3.len = (256 - len)*2;

					break;
				}
			case 0xff1c:
				{
					int vol = (ioregs[0x1c] >> 5) & 3;
					ch3.volume = vol;
					break;
				}
			case 0xff1d:
			case 0xff1e:
				{
					int div = ioregs[0x1d];
					div |= ((int)ioregs[0x1e] & 7) << 8;
					ch3.div_tc = 2048 - div;
					if (addr == 0xff1d) break;
				}
				if (val & 0x80) {
					int vol = (ioregs[0x1c] >> 5) & 3;
					int div = ioregs[0x1d];
					div |= ((int)val & 7) << 8;
					ch3.master = (ioregs[0x1a] & 0x80) > 0;
					ch3.volume = vol;
					ch3.div_tc = 2048 - div;
					ch3.len_enable = (val & 0x40) > 0;
//					printf(" ch3: sft=%02d envd=%d envspd=%d duty=%d len=%02d len_en=%d key=%04d gate=%d%d\n", ch3.volume, ch3.env_dir, ch3.env_speed, ch3.duty, ch3.len, ch3.len_enable, ch3.div_tc, ch3.leftgate, ch3.rightgate);
				}
				break;
			case 0xff1f:
				break;
			case 0xff20:
				{
					int len = ioregs[0x20] & 0x3f;

					ch4.len = (64 - len)*2;

					break;
				}
			case 0xff21:
				{
					int vol = ioregs[0x21] >> 4;
					int envdir = (ioregs[0x21] >> 3) & 1;
					int envspd = ioregs[0x21] & 7;

					ch4.volume = vol;
					ch4.env_dir = envdir;
					ch4.env_speed = ch4.env_speed_tc = envspd*8;
				}
				break;
			case 0xff22:
			case 0xff23:
				{
					int div = ioregs[0x22];
					int shift = div >> 5;
					int rate = div & 7;
					ch4.div = 0;
					ch4.div_tc = 1 << shift;
					if (rate) ch4.div_tc *= rate;
					else ch4.div_tc /= 2;
					if (addr == 0xff22) break;
				}
				if (val & 0x80) {
					int vol = ioregs[0x21] >> 4;
					int envdir = (ioregs[0x21] >> 3) & 1;
					int envspd = ioregs[0x21] & 7;
					int len = ioregs[0x20] & 0x3f;

					ch4.volume = vol;
					ch4.master = 1;
					ch4.env_dir = envdir;
					ch4.env_speed = ch4.env_speed_tc = envspd*8;
					ch4.len = (64 - len)*2;
					ch4.len_enable = (val & 0x40) > 0;

//					printf(" ch4: vol=%02d envd=%d envspd=%d duty=%d len=%02d len_en=%d key=%04d gate=%d%d\n", ch4.volume, ch4.env_dir, ch4.env_speed, ch4.duty, ch4.len, ch4.len_enable, ch4.div_tc, ch4.leftgate, ch4.rightgate);
				}
				break;
			case 0xff24:
				break;
			case 0xff25:
				ch1.leftgate = (val & 0x10) > 0;
				ch1.rightgate = (val & 0x01) > 0;
				ch2.leftgate = (val & 0x20) > 0;
				ch2.rightgate = (val & 0x02) > 0;
				ch3.leftgate = (val & 0x40) > 0;
				ch3.rightgate = (val & 0x04) > 0;
				ch4.leftgate = (val & 0x80) > 0;
				ch4.rightgate = (val & 0x08) > 0;
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
	ram[0x2000 + (addr & 0x1fff)] = val;
}

static void extram_put(unsigned short addr, unsigned char val)
{
	ram[addr & 0x1fff] = val;
}

static put_fn putlookup[256] = {
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&rom_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&vidram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&extram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&intram_put,
	&io_put,
	&io_put
};

static inline void mem_put(unsigned short addr, unsigned char val)
{
	cycles+=4;
	putlookup[addr >> 8](addr, val);
}

static void push(unsigned short val)
{
	unsigned short sp = REGS16_R(regs, SP) - 2;
	REGS16_W(regs, SP, sp);
	mem_put(sp, val & 0xff);
	mem_put(sp+1, val >> 8);
}

static unsigned short pop(void)
{
	unsigned short res;
	unsigned short sp = REGS16_R(regs, SP);

	res  = mem_get(sp);
	res += mem_get(sp+1) << 8;
	REGS16_W(regs, SP, sp + 2);

	return res;
}

static unsigned char get_imm8(void)
{
	unsigned short pc = REGS16_R(regs, PC);
	unsigned char res;
	REGS16_W(regs, PC, pc + 1);
	res = mem_get(pc);
	DPRINTF("%02x", res);
	return res;
}

static unsigned short get_imm16(void)
{
	return get_imm8() + ((unsigned short)get_imm8() << 8);
}

static inline void print_reg(int i)
{
	if (i == 6) DPRINTF("[HL]"); /* indirect memory access by [HL] */
	else DPRINTF("%c", regnames[i]);
}

static unsigned char get_reg(int i)
{
	if (i == 6) /* indirect memory access by [HL] */
		return mem_get(REGS16_R(regs, HL));
	return REGS8_R(regs, i);
}

static void put_reg(int i, unsigned char val)
{
	if (i == 6) /* indirect memory access by [HL] */
		mem_put(REGS16_R(regs, HL), val);
	else REGS8_W(regs, i, val);
}

static void op_unknown(unsigned char op, struct opinfo *oi)
{
	printf(" Unknown opcode %02x.\n", op);
	exit(0);
}

static void op_set(unsigned char op)
{
	int reg = op & 7;
	int bit = (op >> 3) & 7;

	DPRINTF("\tSET %d, ", bit);
	print_reg(reg);
	put_reg(reg, get_reg(reg) | (1 << bit));
}

static void op_res(unsigned char op)
{
	int reg = op & 7;
	int bit = (op >> 3) & 7;

	DPRINTF("\tRES %d, ", bit);
	print_reg(reg);
	put_reg(reg, get_reg(reg) & ~(1 << bit));
}

static void op_bit(unsigned char op)
{
	int reg = op & 7;
	int bit = (op >> 3) & 7;

	DPRINTF("\tBIT %d, ", bit);
	print_reg(reg);
	regs.rn.f &= ~NF;
	regs.rn.f |= HF | ZF;
	regs.rn.f ^= ((get_reg(reg) << 8) >> (bit+1)) & ZF;
}

static void op_rl(unsigned char op, struct opinfo *oi)
{
	int reg = op & 7;
	unsigned short res;
	unsigned char val;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	res  = val = get_reg(reg);
	res  = res << 1;
	res |= (((unsigned short)regs.rn.f & CF) >> 4);
	regs.rn.f = (val >> 7) << 4;
	if (res == 0) regs.rn.f |= ZF;
	put_reg(reg, res);
}

static void op_rla(unsigned char op, struct opinfo *oi)
{
	unsigned short res;

	DPRINTF(" %s", oi->name);
	res  = regs.rn.a;
	res  = res << 1;
	res |= (((unsigned short)regs.rn.f & CF) >> 4);
	regs.rn.f = (regs.rn.a >> 7) << 4;
	if (res == 0) regs.rn.f |= ZF;
	regs.rn.a = res;
}

static void op_rlc(unsigned char op, struct opinfo *oi)
{
	int reg = op & 7;
	unsigned short res;
	unsigned char val;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	res  = val = get_reg(reg);
	res  = res << 1;
	res |= (val >> 7);
	regs.rn.f = (val >> 7) << 4;
	if (res == 0) regs.rn.f |= ZF;
	put_reg(reg, res);
}

static void op_rlca(unsigned char op, struct opinfo *oi)
{
	unsigned short res;

	DPRINTF(" %s", oi->name);
	res  = regs.rn.a;
	res  = res << 1;
	res |= (regs.rn.a >> 7);
	regs.rn.f = (regs.rn.a >> 7) << 4;
	if (res == 0) regs.rn.f |= ZF;
	regs.rn.a = res;
}

static void op_sla(unsigned char op, struct opinfo *oi)
{
	int reg = op & 7;
	unsigned short res;
	unsigned char val;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	res  = val = get_reg(reg);
	res  = res << 1;
	regs.rn.f = ((val >> 7) << 4);
	if (res == 0) regs.rn.f |= ZF;
	put_reg(reg, res);
}

static void op_rr(unsigned char op, struct opinfo *oi)
{
	int reg = op & 7;
	unsigned short res;
	unsigned char val;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	res  = val = get_reg(reg);
	res  = res >> 1;
	res |= (((unsigned short)regs.rn.f & CF) << 3);
	regs.rn.f = (val & 1) << 4;
	if (res == 0) regs.rn.f |= ZF;
	put_reg(reg, res);
}

static void op_rra(unsigned char op, struct opinfo *oi)
{
	unsigned short res;

	DPRINTF(" %s", oi->name);
	res  = regs.rn.a;
	res  = res >> 1;
	res |= (((unsigned short)regs.rn.f & CF) << 3);
	regs.rn.f = (regs.rn.a & 1) << 4;
	if (res == 0) regs.rn.f |= ZF;
	regs.rn.a = res;
}

static void op_rrc(unsigned char op, struct opinfo *oi)
{
	int reg = op & 7;
	unsigned short res;
	unsigned char val;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	res  = val = get_reg(reg);
	res  = res >> 1;
	res |= (val << 7);
	regs.rn.f = (val & 1) << 4;
	if (res == 0) regs.rn.f |= ZF;
	put_reg(reg, res);
}

static void op_rrca(unsigned char op, struct opinfo *oi)
{
	unsigned short res;

	DPRINTF(" %s", oi->name);
	res  = regs.rn.a;
	res  = res >> 1;
	res |= (regs.rn.a << 7);
	regs.rn.f = (regs.rn.a & 1) << 4;
	if (res == 0) regs.rn.f |= ZF;
	regs.rn.a = res;
}

static void op_sra(unsigned char op, struct opinfo *oi)
{
	int reg = op & 7;
	unsigned short res;
	unsigned char val;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	res  = val = get_reg(reg);
	res  = res >> 1;
	res |= (val & 0x80);
	regs.rn.f = ((val & 1) << 4);
	if (res == 0) regs.rn.f |= ZF;
	put_reg(reg, res);
}

static void op_srl(unsigned char op, struct opinfo *oi)
{
	int reg = op & 7;
	unsigned short res;
	unsigned char val;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	res  = val = get_reg(reg);
	res  = res >> 1;
	regs.rn.f = ((val & 1) << 4);
	if (res == 0) regs.rn.f |= ZF;
	put_reg(reg, res);
}

static void op_swap(unsigned char op, struct opinfo *oi)
{
	int reg = op & 7;
	unsigned short res;
	unsigned char val;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	val = get_reg(reg);
	res = (val >> 4) |
	      (val << 4);
	regs.rn.f = 0;
	if (res == 0) regs.rn.f |= ZF;
	put_reg(reg, res);
}

#if DEBUG == 1
#define OPINFO(name, fn) {name, fn}
#else
#define OPINFO(name, fn) {fn}
#endif

static struct opinfo cbops[8] = {
	OPINFO("\tRLC", &op_rlc),		/* opcode cb00-cb07 */
	OPINFO("\tRRC", &op_rrc),		/* opcode cb08-cb0f */
	OPINFO("\tRL", &op_rl),		/* opcode cb10-cb17 */
	OPINFO("\tRR", &op_rr),		/* opcode cb18-cb1f */
	OPINFO("\tSLA", &op_sla),		/* opcode cb20-cb27 */
	OPINFO("\tSRA", &op_sra),		/* opcode cb28-cb2f */
	OPINFO("\tSWAP", &op_swap),		/* opcode cb30-cb37 */
	OPINFO("\tSRL", &op_srl),		/* opcode cb38-cb3f */
};

static void op_cbprefix(unsigned char op, struct opinfo *oi)
{
	unsigned short pc = REGS16_R(regs, PC);

	REGS16_W(regs, PC, pc + 1);
	op = mem_get(pc);
	switch (op >> 6) {
		case 0: cbops[(op >> 3) & 7].fn(op, &cbops[(op >> 3) & 7]);
			return;
		case 1: op_bit(op); return;
		case 2: op_res(op); return;
		case 3: op_set(op); return;
	}
	printf(" Unknown CB subopcode %02x.\n", op);
	exit(0);
}

static void op_ld(unsigned char op, struct opinfo *oi)
{
	int src = op & 7;
	int dst = (op >> 3) & 7;

	DPRINTF(" %s  ", oi->name);
	print_reg(dst);
	DPRINTF(", ");
	print_reg(src);
	put_reg(dst, get_reg(src));
}

static void op_ld_imm(unsigned char op, struct opinfo *oi)
{
	int ofs = get_imm16();

	DPRINTF(" %s  A, [0x%04x]", oi->name, ofs);
	regs.rn.a = mem_get(ofs);
}

static void op_ld_ind16_a(unsigned char op, struct opinfo *oi)
{
	int ofs = get_imm16();

	DPRINTF(" %s  [0x%04x], A", oi->name, ofs);
	mem_put(ofs, regs.rn.a);
}

static void op_ld_ind16_sp(unsigned char op, struct opinfo *oi)
{
	int ofs = get_imm16();
	int sp = REGS16_R(regs, SP);

	DPRINTF(" %s  [0x%04x], SP", oi->name, ofs);
	mem_put(ofs, sp & 0xff);
	mem_put(ofs+1, sp >> 8);
}

static void op_ld_hlsp(unsigned char op, struct opinfo *oi)
{
	char ofs = get_imm8();
	unsigned short old = REGS16_R(regs, SP);
	unsigned short new = old + ofs;

	if (ofs>0) DPRINTF(" %s  HL, SP+0x%02x", oi->name, ofs);
	else DPRINTF(" %s  HL, SP-0x%02x", oi->name, -ofs);
	REGS16_W(regs, HL, new);
	regs.rn.f = 0;
	if (old > new) regs.rn.f |= CF;
	if ((old & 0xfff) > (new & 0xfff)) regs.rn.f |= HF;
}

static void op_ld_sphl(unsigned char op, struct opinfo *oi)
{
	DPRINTF(" %s  SP, HL", oi->name);
	REGS16_W(regs, SP, REGS16_R(regs, HL));
}

static void op_ld_reg16_imm(unsigned char op, struct opinfo *oi)
{
	int val = get_imm16();
	int reg = (op >> 4) & 3;

	reg += reg > 2; /* skip over FA */
	DPRINTF(" %s  %s, 0x%02x", oi->name, regnamech16[reg], val);
	REGS16_W(regs, reg, val);
}

static void op_ld_reg16_a(unsigned char op, struct opinfo *oi)
{
	int reg = (op >> 4) & 3;
	unsigned short r;

	reg -= reg > 2;
	if (op & 8) {
		DPRINTF(" %s  A, [%s]", oi->name, regnamech16[reg]);
		regs.rn.a = mem_get(r = REGS16_R(regs, reg));
	} else {
		DPRINTF(" %s  [%s], A", oi->name, regnamech16[reg]);
		mem_put(r = REGS16_R(regs, reg), regs.rn.a);
	}

	if (reg == 2) {
		r += (((op & 0x10) == 0) << 1)-1;
		REGS16_W(regs, reg, r);
	}
}

static void op_ld_reg8_imm(unsigned char op, struct opinfo *oi)
{
	int val = get_imm8();
	int reg = (op >> 3) & 7;

	DPRINTF(" %s  ", oi->name);
	print_reg(reg);
	put_reg(reg, val);
	DPRINTF(", 0x%02x", val);
}

static void op_ldh(unsigned char op, struct opinfo *oi)
{
	int ofs = op & 2 ? 0 : get_imm8();

	if (op & 0x10) {
		DPRINTF(" %s  A, ", oi->name);
		if ((op & 2) == 0) {
			DPRINTF("[%02x]", ofs);
		} else {
			ofs = regs.rn.c;
			DPRINTF("[C]");
		}
		regs.rn.a = mem_get(0xff00 + ofs);
	} else {
		if ((op & 2) == 0) {
			DPRINTF(" %s  [%02x], A", oi->name, ofs);
		} else {
			ofs = regs.rn.c;
			DPRINTF(" %s  [C], A", oi->name);
		}
		mem_put(0xff00 + ofs, regs.rn.a);
	}
}

static void op_inc(unsigned char op, struct opinfo *oi)
{
	int reg = (op >> 3) & 7;
	unsigned char res;
	unsigned char old;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	old = res = get_reg(reg);
	res++;
	put_reg(reg, res);
	regs.rn.f &= ~(NF | ZF | HF);
	if (res == 0) regs.rn.f |= ZF;
	if ((old & 15) > (res & 15)) regs.rn.f |= HF;
}

static void op_inc16(unsigned char op, struct opinfo *oi)
{
	int reg = (op >> 4) & 3;
	unsigned short res = REGS16_R(regs, reg);

	DPRINTF(" %s %s\t", oi->name, regnamech16[reg]);
	res++;
	REGS16_W(regs, reg, res);
}

static void op_dec(unsigned char op, struct opinfo *oi)
{
	int reg = (op >> 3) & 7;
	unsigned char res;
	unsigned char old;

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	old = res = get_reg(reg);
	res--;
	put_reg(reg, res);
	regs.rn.f |= NF;
	regs.rn.f &= ~(ZF | HF);
	if (res == 0) regs.rn.f |= ZF;
	if ((old & 15) > (res & 15)) regs.rn.f |= HF;
}

static void op_dec16(unsigned char op, struct opinfo *oi)
{
	int reg = (op >> 4) & 3;
	unsigned short res = REGS16_R(regs, reg);

	DPRINTF(" %s %s", oi->name, regnamech16[reg]);
	res--;
	REGS16_W(regs, reg, res);
}

static void op_add_sp_imm(unsigned char op, struct opinfo *oi)
{
	char imm = get_imm8();
	unsigned short old = REGS16_R(regs, SP);
	unsigned short new = old;

	DPRINTF(" %s SP, %02x", oi->name, imm);
	new += imm;
	REGS16_W(regs, SP, new);
	regs.rn.f = 0;
	if (old > new) regs.rn.f |= CF;
	if ((old & 0xfff) > (new & 0xfff)) regs.rn.f |= HF;
}

static void op_add(unsigned char op, struct opinfo *oi)
{
	unsigned char old = regs.rn.a;
	unsigned char new;

	DPRINTF(" %s A, ", oi->name);
	print_reg(op & 7);
	regs.rn.a += get_reg(op & 7);
	new = regs.rn.a;
	regs.rn.f = 0;
	if (old > new) regs.rn.f |= CF;
	if ((old & 15) > (new & 15)) regs.rn.f |= HF;
	if (new == 0) regs.rn.f |= ZF;
}

static void op_add_imm(unsigned char op, struct opinfo *oi)
{
	unsigned char imm = get_imm8();
	unsigned char old = regs.rn.a;
	unsigned char new = old;

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	new += imm;
	regs.rn.a = new;
	regs.rn.f = 0;
	if (old > new) regs.rn.f |= CF;
	if ((old & 15) > (new & 15)) regs.rn.f |= HF;
	if (new == 0) regs.rn.f |= ZF;
}

static void op_add_hl(unsigned char op, struct opinfo *oi)
{
	int reg = (op >> 4) & 3;
	unsigned short old = REGS16_R(regs, HL);
	unsigned short new = old;

	reg += reg > 2;
	DPRINTF(" %s HL, %s", oi->name, regnamech16[reg]);

	new += REGS16_R(regs, reg);
	REGS16_W(regs, HL, new);

	regs.rn.f &= ~(NF | CF | HF);
	if (old > new) regs.rn.f |= CF;
	if ((old & 0xfff) > (new & 0xfff)) regs.rn.f |= HF;
}

static void op_adc(unsigned char op, struct opinfo *oi)
{
	unsigned char old = regs.rn.a;
	unsigned char new;

	DPRINTF(" %s A, ", oi->name);
	print_reg(op & 7);
	regs.rn.a += get_reg(op & 7);
	regs.rn.a += (regs.rn.f & CF) > 0;
	regs.rn.f &= ~NF;
	new = regs.rn.a;
	if (old > new) regs.rn.f |= CF; else regs.rn.f &= ~CF;
	if ((old & 15) > (new & 15)) regs.rn.f |= HF; else regs.rn.f &= ~HF;
	if (new == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_adc_imm(unsigned char op, struct opinfo *oi)
{
	unsigned char imm = get_imm8();
	unsigned char old = regs.rn.a;
	unsigned char new = old;

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	new += imm;
	new += (regs.rn.f & CF) > 0;
	regs.rn.f &= ~NF;
	regs.rn.a = new;
	if (old > new) regs.rn.f |= CF; else regs.rn.f &= ~CF;
	if ((old & 15) > (new & 15)) regs.rn.f |= HF; else regs.rn.f &= ~HF;
	if (new == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_cp(unsigned char op, struct opinfo *oi)
{
	unsigned char old = regs.rn.a;
	unsigned char new = old;

	DPRINTF(" %s A, ", oi->name);
	print_reg(op & 7);
	new -= get_reg(op & 7);
	regs.rn.f = NF;
	if (old < new) regs.rn.f |= CF;
	if ((old & 15) < (new & 15)) regs.rn.f |= HF;
	if (new == 0) regs.rn.f |= ZF;
}

static void op_cp_imm(unsigned char op, struct opinfo *oi)
{
	unsigned char imm = get_imm8();
	unsigned char old = regs.rn.a;
	unsigned char new = old;

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	new -= imm;
	regs.rn.f = NF;
	if (old < new) regs.rn.f |= CF;
	if ((old & 15) < (new & 15)) regs.rn.f |= HF;
	if (new == 0) regs.rn.f |= ZF;
}

static void op_sub(unsigned char op, struct opinfo *oi)
{
	unsigned char old = regs.rn.a;
	unsigned char new;

	DPRINTF(" %s A, ", oi->name);
	print_reg(op & 7);
	regs.rn.a -= get_reg(op & 7);
	new = regs.rn.a;
	regs.rn.f = NF;
	if (old < new) regs.rn.f |= CF;
	if ((old & 15) < (new & 15)) regs.rn.f |= HF;
	if (new == 0) regs.rn.f |= ZF;
}

static void op_sub_imm(unsigned char op, struct opinfo *oi)
{
	unsigned char imm = get_imm8();
	unsigned char old = regs.rn.a;
	unsigned char new = old;

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	new -= imm;
	regs.rn.a = new;
	regs.rn.f = NF;
	if (old < new) regs.rn.f |= CF;
	if ((old & 15) < (new & 15)) regs.rn.f |= HF;
	if (new == 0) regs.rn.f |= ZF;
}

static void op_sbc(unsigned char op, struct opinfo *oi)
{
	unsigned char old = regs.rn.a;
	unsigned char new;

	DPRINTF(" %s A, ", oi->name);
	print_reg(op & 7);
	regs.rn.a -= get_reg(op & 7);
	regs.rn.a -= (regs.rn.f & CF) > 0;
	new = regs.rn.a;
	regs.rn.f = NF;
	if (old < new) regs.rn.f |= CF;
	if ((old & 15) < (new & 15)) regs.rn.f |= HF;
	if (new == 0) regs.rn.f |= ZF;
}

static void op_sbc_imm(unsigned char op, struct opinfo *oi)
{
	unsigned char imm = get_imm8();
	unsigned char old = regs.rn.a;
	unsigned char new = old;

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	new -= imm;
	new -= (regs.rn.f & CF) > 0;
	regs.rn.a = new;
	regs.rn.f = NF;
	if (old < new) regs.rn.f |= CF;
	if ((old & 15) < (new & 15)) regs.rn.f |= HF;
	if (new == 0) regs.rn.f |= ZF;
}

static void op_and(unsigned char op, struct opinfo *oi)
{
	DPRINTF(" %s A, ", oi->name);
	print_reg(op & 7);
	regs.rn.a &= get_reg(op & 7);
	regs.rn.f = HF;
	if (regs.rn.a == 0) regs.rn.f |= ZF;
}

static void op_and_imm(unsigned char op, struct opinfo *oi)
{
	unsigned char imm = get_imm8();

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	regs.rn.a &= imm;
	regs.rn.f = HF;
	if (regs.rn.a == 0) regs.rn.f |= ZF;
}

static void op_or(unsigned char op, struct opinfo *oi)
{
	DPRINTF(" %s A, ", oi->name);
	print_reg(op & 7);
	regs.rn.a |= get_reg(op & 7);
	regs.rn.f = 0;
	if (regs.rn.a == 0) regs.rn.f |= ZF;
}

static void op_or_imm(unsigned char op, struct opinfo *oi)
{
	unsigned char imm = get_imm8();

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	regs.rn.a |= imm;
	regs.rn.f = 0;
	if (regs.rn.a == 0) regs.rn.f |= ZF;
}

static void op_xor(unsigned char op, struct opinfo *oi)
{
	DPRINTF(" %s A, ", oi->name);
	print_reg(op & 7);
	regs.rn.a ^= get_reg(op & 7);
	regs.rn.f = 0;
	if (regs.rn.a == 0) regs.rn.f |= ZF;
}

static void op_xor_imm(unsigned char op, struct opinfo *oi)
{
	unsigned char imm = get_imm8();

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	regs.rn.a ^= imm;
	regs.rn.f = 0;
	if (regs.rn.a == 0) regs.rn.f |= ZF;
}

static void op_push(unsigned char op, struct opinfo *oi)
{
	int reg = op >> 4 & 3;

	push(REGS16_R(regs, reg));
	DPRINTF(" %s %s\t", oi->name, regnamech16[reg]);
}

static void op_pop(unsigned char op, struct opinfo *oi)
{
	int reg = op >> 4 & 3;

	REGS16_W(regs, reg, pop());
	DPRINTF(" %s %s\t", oi->name, regnamech16[reg]);
}

static void op_cpl(unsigned char op, struct opinfo *oi)
{
	DPRINTF(" %s", oi->name);
	regs.rn.a = ~regs.rn.a;
	regs.rn.f |= NF | HF;
}

static void op_ccf(unsigned char op, struct opinfo *oi)
{
	DPRINTF(" %s", oi->name);
	regs.rn.f ^= CF;
	regs.rn.f &= ~(NF | HF);
}

static void op_scf(unsigned char op, struct opinfo *oi)
{
	DPRINTF(" %s", oi->name);
	regs.rn.f |= CF;
	regs.rn.f &= ~(NF | HF);
}

#if DEBUG == 1
static char *conds[4] = {
	"NZ", "Z", "NC", "C"
};
#endif

static void op_call(unsigned char op, struct opinfo *oi)
{
	unsigned short ofs = get_imm16();

	DPRINTF(" %s 0x%04x", oi->name, ofs);
	push(REGS16_R(regs, PC));
	REGS16_W(regs, PC, ofs);
}

static void op_call_cond(unsigned char op, struct opinfo *oi)
{
	unsigned short ofs = get_imm16();
	int cond = (op >> 3) & 3;

	DPRINTF(" %s %s 0x%04x", oi->name, conds[cond], ofs);
	switch (cond) {
		case 0: if ((regs.rn.f & ZF) != 0) return; break;
		case 1: if ((regs.rn.f & ZF) == 0) return; break;
		case 2: if ((regs.rn.f & CF) != 0) return; break;
		case 3: if ((regs.rn.f & CF) == 0) return; break;
	}
	push(REGS16_R(regs, PC));
	REGS16_W(regs, PC, ofs);
}

static void op_ret(unsigned char op, struct opinfo *oi)
{
	REGS16_W(regs, PC, pop());
	DPRINTF(" %s", oi->name);
}

static void op_reti(unsigned char op, struct opinfo *oi)
{
	REGS16_W(regs, PC, pop());
	DPRINTF(" %s", oi->name);
}

static void op_ret_cond(unsigned char op, struct opinfo *oi)
{
	int cond = (op >> 3) & 3;

	DPRINTF(" %s %s", oi->name, conds[cond]);
	switch (cond) {
		case 0: if ((regs.rn.f & ZF) != 0) return; break;
		case 1: if ((regs.rn.f & ZF) == 0) return; break;
		case 2: if ((regs.rn.f & CF) != 0) return; break;
		case 3: if ((regs.rn.f & CF) == 0) return; break;
	}
	REGS16_W(regs, PC, pop());
}

static void op_halt(unsigned char op, struct opinfo *oi)
{
	halted = 1;
	DPRINTF(" %s", oi->name);
}

static void op_stop(unsigned char op, struct opinfo *oi)
{
	DPRINTF(" %s", oi->name);
}

static void op_di(unsigned char op, struct opinfo *oi)
{
	interrupts = 0;
	DPRINTF(" %s", oi->name);
}

static void op_ei(unsigned char op, struct opinfo *oi)
{
	interrupts = 1;
	DPRINTF(" %s", oi->name);
}

static void op_jr(unsigned char op, struct opinfo *oi)
{
	short ofs = (char) get_imm8();

	if (ofs < 0) DPRINTF(" %s $-0x%02x", oi->name, -ofs);
	else DPRINTF(" %s $+0x%02x", oi->name, ofs);
	REGS16_W(regs, PC, REGS16_R(regs, PC) + ofs);
}

static void op_jr_cond(unsigned char op, struct opinfo *oi)
{
	short ofs = (char) get_imm8();
	int cond = (op >> 3) & 3;

	if (ofs < 0) DPRINTF(" %s %s $-0x%02x", oi->name, conds[cond], -ofs);
	else DPRINTF(" %s %s $+0x%02x", oi->name, conds[cond], ofs);
	switch (cond) {
		case 0: if ((regs.rn.f & ZF) != 0) return; break;
		case 1: if ((regs.rn.f & ZF) == 0) return; break;
		case 2: if ((regs.rn.f & CF) != 0) return; break;
		case 3: if ((regs.rn.f & CF) == 0) return; break;
	}
	REGS16_W(regs, PC, REGS16_R(regs, PC) + ofs);
}

static void op_jp(unsigned char op, struct opinfo *oi)
{
	unsigned short ofs = get_imm16();

	DPRINTF(" %s 0x%04x", oi->name, ofs);
	REGS16_W(regs, PC, ofs);
}

static void op_jp_hl(unsigned char op, struct opinfo *oi)
{
	DPRINTF(" %s HL", oi->name);
	REGS16_W(regs, PC, REGS16_R(regs, HL));
}

static void op_jp_cond(unsigned char op, struct opinfo *oi)
{
	unsigned short ofs = get_imm16();
	int cond = (op >> 3) & 3;

	DPRINTF(" %s %s 0x%04x", oi->name, conds[cond], ofs);
	switch (cond) {
		case 0: if ((regs.rn.f & ZF) != 0) return; break;
		case 1: if ((regs.rn.f & ZF) == 0) return; break;
		case 2: if ((regs.rn.f & CF) != 0) return; break;
		case 3: if ((regs.rn.f & CF) == 0) return; break;
	}
	REGS16_W(regs, PC, ofs);
}

static void op_rst(unsigned char op, struct opinfo *oi)
{
	short ofs = op & 0x38;

	DPRINTF(" %s 0x%02x", oi->name, ofs);
	push(REGS16_R(regs, PC));
	REGS16_W(regs, PC, ofs);
}

static void op_nop(unsigned char op, struct opinfo *oi)
{
	DPRINTF(" %s", oi->name);
}

static struct opinfo ops[256] = {
	OPINFO("\tNOP", &op_nop),		/* opcode 00 */
	OPINFO("\tLD", &op_ld_reg16_imm),		/* opcode 01 */
	OPINFO("\tLD", &op_ld_reg16_a),		/* opcode 02 */
	OPINFO("\tINC", &op_inc16),		/* opcode 03 */
	OPINFO("\tINC", &op_inc),		/* opcode 04 */
	OPINFO("\tDEC", &op_dec),		/* opcode 05 */
	OPINFO("\tLD", &op_ld_reg8_imm),		/* opcode 06 */
	OPINFO("\tRLCA", &op_rlca),		/* opcode 07 */
	OPINFO("\tLD", &op_ld_ind16_sp),		/* opcode 08 */
	OPINFO("\tADD", &op_add_hl),		/* opcode 09 */
	OPINFO("\tLD", &op_ld_reg16_a),		/* opcode 0a */
	OPINFO("\tDEC", &op_dec16),		/* opcode 0b */
	OPINFO("\tINC", &op_inc),		/* opcode 0c */
	OPINFO("\tDEC", &op_dec),		/* opcode 0d */
	OPINFO("\tLD", &op_ld_reg8_imm),		/* opcode 0e */
	OPINFO("\tRRCA", &op_rrca),		/* opcode 0f */
	OPINFO("\tSTOP", &op_stop),		/* opcode 10 */
	OPINFO("\tLD", &op_ld_reg16_imm),		/* opcode 11 */
	OPINFO("\tLD", &op_ld_reg16_a),		/* opcode 12 */
	OPINFO("\tINC", &op_inc16),		/* opcode 13 */
	OPINFO("\tINC", &op_inc),		/* opcode 14 */
	OPINFO("\tDEC", &op_dec),		/* opcode 15 */
	OPINFO("\tLD", &op_ld_reg8_imm),		/* opcode 16 */
	OPINFO("\tRLA", &op_rla),		/* opcode 17 */
	OPINFO("\tJR", &op_jr),		/* opcode 18 */
	OPINFO("\tADD", &op_add_hl),		/* opcode 19 */
	OPINFO("\tLD", &op_ld_reg16_a),		/* opcode 1a */
	OPINFO("\tDEC", &op_dec16),		/* opcode 1b */
	OPINFO("\tINC", &op_inc),		/* opcode 1c */
	OPINFO("\tDEC", &op_dec),		/* opcode 1d */
	OPINFO("\tLD", &op_ld_reg8_imm),		/* opcode 1e */
	OPINFO("\tRRA", &op_rra),		/* opcode 1f */
	OPINFO("\tJR", &op_jr_cond),		/* opcode 20 */
	OPINFO("\tLD", &op_ld_reg16_imm),		/* opcode 21 */
	OPINFO("\tLDI", &op_ld_reg16_a),		/* opcode 22 */
	OPINFO("\tINC", &op_inc16),		/* opcode 23 */
	OPINFO("\tINC", &op_inc),		/* opcode 24 */
	OPINFO("\tDEC", &op_dec),		/* opcode 25 */
	OPINFO("\tLD", &op_ld_reg8_imm),		/* opcode 26 */
	OPINFO("\tUNKN", &op_unknown),		/* opcode 27 */
	OPINFO("\tJR", &op_jr_cond),		/* opcode 28 */
	OPINFO("\tADD", &op_add_hl),		/* opcode 29 */
	OPINFO("\tLDI", &op_ld_reg16_a),		/* opcode 2a */
	OPINFO("\tDEC", &op_dec16),		/* opcode 2b */
	OPINFO("\tINC", &op_inc),		/* opcode 2c */
	OPINFO("\tDEC", &op_dec),		/* opcode 2d */
	OPINFO("\tLD", &op_ld_reg8_imm),		/* opcode 2e */
	OPINFO("\tCPL", &op_cpl),		/* opcode 2f */
	OPINFO("\tJR", &op_jr_cond),		/* opcode 30 */
	OPINFO("\tLD", &op_ld_reg16_imm),		/* opcode 31 */
	OPINFO("\tLDD", &op_ld_reg16_a),		/* opcode 32 */
	OPINFO("\tINC", &op_inc16),		/* opcode 33 */
	OPINFO("\tINC", &op_inc),		/* opcode 34 */
	OPINFO("\tDEC", &op_dec),		/* opcode 35 */
	OPINFO("\tLD", &op_ld_reg8_imm),		/* opcode 36 */
	OPINFO("\tSCF", &op_scf),		/* opcode 37 */
	OPINFO("\tJR", &op_jr_cond),		/* opcode 38 */
	OPINFO("\tADD", &op_add_hl),		/* opcode 39 */
	OPINFO("\tLDD", &op_ld_reg16_a),		/* opcode 3a */
	OPINFO("\tDEC", &op_dec16),		/* opcode 3b */
	OPINFO("\tINC", &op_inc),		/* opcode 3c */
	OPINFO("\tDEC", &op_dec),		/* opcode 3d */
	OPINFO("\tLD", &op_ld_reg8_imm),		/* opcode 3e */
	OPINFO("\tCCF", &op_ccf),		/* opcode 3f */
	OPINFO("\tLD", &op_ld),		/* opcode 40 */
	OPINFO("\tLD", &op_ld),		/* opcode 41 */
	OPINFO("\tLD", &op_ld),		/* opcode 42 */
	OPINFO("\tLD", &op_ld),		/* opcode 43 */
	OPINFO("\tLD", &op_ld),		/* opcode 44 */
	OPINFO("\tLD", &op_ld),		/* opcode 45 */
	OPINFO("\tLD", &op_ld),		/* opcode 46 */
	OPINFO("\tLD", &op_ld),		/* opcode 47 */
	OPINFO("\tLD", &op_ld),		/* opcode 48 */
	OPINFO("\tLD", &op_ld),		/* opcode 49 */
	OPINFO("\tLD", &op_ld),		/* opcode 4a */
	OPINFO("\tLD", &op_ld),		/* opcode 4b */
	OPINFO("\tLD", &op_ld),		/* opcode 4c */
	OPINFO("\tLD", &op_ld),		/* opcode 4d */
	OPINFO("\tLD", &op_ld),		/* opcode 4e */
	OPINFO("\tLD", &op_ld),		/* opcode 4f */
	OPINFO("\tLD", &op_ld),		/* opcode 50 */
	OPINFO("\tLD", &op_ld),		/* opcode 51 */
	OPINFO("\tLD", &op_ld),		/* opcode 52 */
	OPINFO("\tLD", &op_ld),		/* opcode 53 */
	OPINFO("\tLD", &op_ld),		/* opcode 54 */
	OPINFO("\tLD", &op_ld),		/* opcode 55 */
	OPINFO("\tLD", &op_ld),		/* opcode 56 */
	OPINFO("\tLD", &op_ld),		/* opcode 57 */
	OPINFO("\tLD", &op_ld),		/* opcode 58 */
	OPINFO("\tLD", &op_ld),		/* opcode 59 */
	OPINFO("\tLD", &op_ld),		/* opcode 5a */
	OPINFO("\tLD", &op_ld),		/* opcode 5b */
	OPINFO("\tLD", &op_ld),		/* opcode 5c */
	OPINFO("\tLD", &op_ld),		/* opcode 5d */
	OPINFO("\tLD", &op_ld),		/* opcode 5e */
	OPINFO("\tLD", &op_ld),		/* opcode 5f */
	OPINFO("\tLD", &op_ld),		/* opcode 60 */
	OPINFO("\tLD", &op_ld),		/* opcode 61 */
	OPINFO("\tLD", &op_ld),		/* opcode 62 */
	OPINFO("\tLD", &op_ld),		/* opcode 63 */
	OPINFO("\tLD", &op_ld),		/* opcode 64 */
	OPINFO("\tLD", &op_ld),		/* opcode 65 */
	OPINFO("\tLD", &op_ld),		/* opcode 66 */
	OPINFO("\tLD", &op_ld),		/* opcode 67 */
	OPINFO("\tLD", &op_ld),		/* opcode 68 */
	OPINFO("\tLD", &op_ld),		/* opcode 69 */
	OPINFO("\tLD", &op_ld),		/* opcode 6a */
	OPINFO("\tLD", &op_ld),		/* opcode 6b */
	OPINFO("\tLD", &op_ld),		/* opcode 6c */
	OPINFO("\tLD", &op_ld),		/* opcode 6d */
	OPINFO("\tLD", &op_ld),		/* opcode 6e */
	OPINFO("\tLD", &op_ld),		/* opcode 6f */
	OPINFO("\tLD", &op_ld),		/* opcode 70 */
	OPINFO("\tLD", &op_ld),		/* opcode 71 */
	OPINFO("\tLD", &op_ld),		/* opcode 72 */
	OPINFO("\tLD", &op_ld),		/* opcode 73 */
	OPINFO("\tLD", &op_ld),		/* opcode 74 */
	OPINFO("\tLD", &op_ld),		/* opcode 75 */
	OPINFO("\tHALT", &op_halt),		/* opcode 76 */
	OPINFO("\tLD", &op_ld),		/* opcode 77 */
	OPINFO("\tLD", &op_ld),		/* opcode 78 */
	OPINFO("\tLD", &op_ld),		/* opcode 79 */
	OPINFO("\tLD", &op_ld),		/* opcode 7a */
	OPINFO("\tLD", &op_ld),		/* opcode 7b */
	OPINFO("\tLD", &op_ld),		/* opcode 7c */
	OPINFO("\tLD", &op_ld),		/* opcode 7d */
	OPINFO("\tLD", &op_ld),		/* opcode 7e */
	OPINFO("\tLD", &op_ld),		/* opcode 7f */
	OPINFO("\tADD", &op_add),		/* opcode 80 */
	OPINFO("\tADD", &op_add),		/* opcode 81 */
	OPINFO("\tADD", &op_add),		/* opcode 82 */
	OPINFO("\tADD", &op_add),		/* opcode 83 */
	OPINFO("\tADD", &op_add),		/* opcode 84 */
	OPINFO("\tADD", &op_add),		/* opcode 85 */
	OPINFO("\tADD", &op_add),		/* opcode 86 */
	OPINFO("\tADD", &op_add),		/* opcode 87 */
	OPINFO("\tADC", &op_adc),		/* opcode 88 */
	OPINFO("\tADC", &op_adc),		/* opcode 89 */
	OPINFO("\tADC", &op_adc),		/* opcode 8a */
	OPINFO("\tADC", &op_adc),		/* opcode 8b */
	OPINFO("\tADC", &op_adc),		/* opcode 8c */
	OPINFO("\tADC", &op_adc),		/* opcode 8d */
	OPINFO("\tADC", &op_adc),		/* opcode 8e */
	OPINFO("\tADC", &op_adc),		/* opcode 8f */
	OPINFO("\tSUB", &op_sub),		/* opcode 90 */
	OPINFO("\tSUB", &op_sub),		/* opcode 91 */
	OPINFO("\tSUB", &op_sub),		/* opcode 92 */
	OPINFO("\tSUB", &op_sub),		/* opcode 93 */
	OPINFO("\tSUB", &op_sub),		/* opcode 94 */
	OPINFO("\tSUB", &op_sub),		/* opcode 95 */
	OPINFO("\tSUB", &op_sub),		/* opcode 96 */
	OPINFO("\tSUB", &op_sub),		/* opcode 97 */
	OPINFO("\tSBC", &op_sbc),		/* opcode 98 */
	OPINFO("\tSBC", &op_sbc),		/* opcode 99 */
	OPINFO("\tSBC", &op_sbc),		/* opcode 9a */
	OPINFO("\tSBC", &op_sbc),		/* opcode 9b */
	OPINFO("\tSBC", &op_sbc),		/* opcode 9c */
	OPINFO("\tSBC", &op_sbc),		/* opcode 9d */
	OPINFO("\tSBC", &op_sbc),		/* opcode 9e */
	OPINFO("\tSBC", &op_sbc),		/* opcode 9f */
	OPINFO("\tAND", &op_and),		/* opcode a0 */
	OPINFO("\tAND", &op_and),		/* opcode a1 */
	OPINFO("\tAND", &op_and),		/* opcode a2 */
	OPINFO("\tAND", &op_and),		/* opcode a3 */
	OPINFO("\tAND", &op_and),		/* opcode a4 */
	OPINFO("\tAND", &op_and),		/* opcode a5 */
	OPINFO("\tAND", &op_and),		/* opcode a6 */
	OPINFO("\tAND", &op_and),		/* opcode a7 */
	OPINFO("\tXOR", &op_xor),		/* opcode a8 */
	OPINFO("\tXOR", &op_xor),		/* opcode a9 */
	OPINFO("\tXOR", &op_xor),		/* opcode aa */
	OPINFO("\tXOR", &op_xor),		/* opcode ab */
	OPINFO("\tXOR", &op_xor),		/* opcode ac */
	OPINFO("\tXOR", &op_xor),		/* opcode ad */
	OPINFO("\tXOR", &op_xor),		/* opcode ae */
	OPINFO("\tXOR", &op_xor),		/* opcode af */
	OPINFO("\tOR", &op_or),		/* opcode b0 */
	OPINFO("\tOR", &op_or),		/* opcode b1 */
	OPINFO("\tOR", &op_or),		/* opcode b2 */
	OPINFO("\tOR", &op_or),		/* opcode b3 */
	OPINFO("\tOR", &op_or),		/* opcode b4 */
	OPINFO("\tOR", &op_or),		/* opcode b5 */
	OPINFO("\tOR", &op_or),		/* opcode b6 */
	OPINFO("\tOR", &op_or),		/* opcode b7 */
	OPINFO("\tCP", &op_cp),		/* opcode b8 */
	OPINFO("\tCP", &op_cp),		/* opcode b9 */
	OPINFO("\tCP", &op_cp),		/* opcode ba */
	OPINFO("\tCP", &op_cp),		/* opcode bb */
	OPINFO("\tCP", &op_cp),		/* opcode bc */
	OPINFO("\tCP", &op_cp),		/* opcode bd */
	OPINFO("\tCP", &op_cp),		/* opcode be */
	OPINFO("\tUNKN", &op_unknown),		/* opcode bf */
	OPINFO("\tRET", &op_ret_cond),		/* opcode c0 */
	OPINFO("\tPOP", &op_pop),		/* opcode c1 */
	OPINFO("\tJP", &op_jp_cond),		/* opcode c2 */
	OPINFO("\tJP", &op_jp),		/* opcode c3 */
	OPINFO("\tCALL", &op_call_cond),		/* opcode c4 */
	OPINFO("\tPUSH", &op_push),		/* opcode c5 */
	OPINFO("\tADD", &op_add_imm),		/* opcode c6 */
	OPINFO("\tRST", &op_rst),		/* opcode c7 */
	OPINFO("\tRET", &op_ret_cond),		/* opcode c8 */
	OPINFO("\tRET", &op_ret),		/* opcode c9 */
	OPINFO("\tJP", &op_jp_cond),		/* opcode ca */
	OPINFO("\tCBPREFIX", &op_cbprefix),		/* opcode cb */
	OPINFO("\tCALL", &op_call_cond),		/* opcode cc */
	OPINFO("\tCALL", &op_call),		/* opcode cd */
	OPINFO("\tADC", &op_adc_imm),		/* opcode ce */
	OPINFO("\tRST", &op_rst),		/* opcode cf */
	OPINFO("\tRET", &op_ret_cond),		/* opcode d0 */
	OPINFO("\tPOP", &op_pop),		/* opcode d1 */
	OPINFO("\tJP", &op_jp_cond),		/* opcode d2 */
	OPINFO("\tUNKN", &op_unknown),		/* opcode d3 */
	OPINFO("\tCALL", &op_call_cond),		/* opcode d4 */
	OPINFO("\tPUSH", &op_push),		/* opcode d5 */
	OPINFO("\tSUB", &op_sub_imm),		/* opcode d6 */
	OPINFO("\tRST", &op_rst),		/* opcode d7 */
	OPINFO("\tRET", &op_ret_cond),		/* opcode d8 */
	OPINFO("\tRETI", &op_reti),		/* opcode d9 */
	OPINFO("\tJP", &op_jp_cond),		/* opcode da */
	OPINFO("\tUNKN", &op_unknown),		/* opcode db */
	OPINFO("\tCALL", &op_call_cond),		/* opcode dc */
	OPINFO("\tUNKN", &op_unknown),		/* opcode dd */
	OPINFO("\tSBC", &op_sbc_imm),		/* opcode de */
	OPINFO("\tRST", &op_rst),		/* opcode df */
	OPINFO("\tLDH", &op_ldh),		/* opcode e0 */
	OPINFO("\tPOP", &op_pop),		/* opcode e1 */
	OPINFO("\tLDH", &op_ldh),		/* opcode e2 */
	OPINFO("\tUNKN", &op_unknown),		/* opcode e3 */
	OPINFO("\tUNKN", &op_unknown),		/* opcode e4 */
	OPINFO("\tPUSH", &op_push),		/* opcode e5 */
	OPINFO("\tAND", &op_and_imm),		/* opcode e6 */
	OPINFO("\tRST", &op_rst),		/* opcode e7 */
	OPINFO("\tADD", &op_add_sp_imm),		/* opcode e8 */
	OPINFO("\tJP", &op_jp_hl),		/* opcode e9 */
	OPINFO("\tLD", &op_ld_ind16_a),		/* opcode ea */
	OPINFO("\tUNKN", &op_unknown),		/* opcode eb */
	OPINFO("\tUNKN", &op_unknown),		/* opcode ec */
	OPINFO("\tUNKN", &op_unknown),		/* opcode ed */
	OPINFO("\tXOR", &op_xor_imm),		/* opcode ee */
	OPINFO("\tRST", &op_rst),		/* opcode ef */
	OPINFO("\tLDH", &op_ldh),		/* opcode f0 */
	OPINFO("\tPOP", &op_pop),		/* opcode f1 */
	OPINFO("\tLDH", &op_ldh),		/* opcode f2 */
	OPINFO("\tDI", &op_di),		/* opcode f3 */
	OPINFO("\tUNKN", &op_unknown),		/* opcode f4 */
	OPINFO("\tPUSH", &op_push),		/* opcode f5 */
	OPINFO("\tOR", &op_or_imm),		/* opcode f6 */
	OPINFO("\tRST", &op_rst),		/* opcode f7 */
	OPINFO("\tLD", &op_ld_hlsp),		/* opcode f8 */
	OPINFO("\tLD", &op_ld_sphl),		/* opcode f9 */
	OPINFO("\tLD", &op_ld_imm),		/* opcode fa */
	OPINFO("\tEI", &op_ei),		/* opcode fb */
	OPINFO("\tUNKN", &op_unknown),		/* opcode fc */
	OPINFO("\tUNKN", &op_unknown),		/* opcode fd */
	OPINFO("\tCP", &op_cp_imm),		/* opcode fe */
	OPINFO("\tRST", &op_rst),		/* opcode ff */
};

static void dump_regs(void)
{
	int i;

	DPRINTF("; ");
	for (i=0; i<8; i++) {
		DPRINTF("%c=%02x ", regnames[i], REGS8_R(regs, i));
	}
	for (i=5; i<6; i++) {
		DPRINTF("%s=%04x ", regnamech16[i], REGS16_R(regs, i));
	}
	DPRINTF("\n");
}

#if DEBUG == 1
static void show_reg_diffs(void)
{
	int i;

	DPRINTF("\t\t; ");
	for (i=0; i<3; i++) {
		if (REGS16_R(regs, i) != REGS16_R(oldregs, i)) {
			DPRINTF("%s=%04x ", regnamech16[i], REGS16_R(regs, i));
			REGS16_W(oldregs, i, REGS16_R(regs, i));
		}
	}
	for (i=6; i<8; i++) {
		if (REGS8_R(regs, i) != REGS8_R(oldregs, i)) {
			if (i == 6) { /* Flags */
				if (regs.rn.f & ZF) DPRINTF("Z");
				else DPRINTF("z");
				if (regs.rn.f & NF) DPRINTF("N");
				else DPRINTF("n");
				if (regs.rn.f & HF) DPRINTF("H");
				else DPRINTF("h");
				if (regs.rn.f & CF) DPRINTF("C");
				else DPRINTF("c");
				DPRINTF(" ");
			} else {
				DPRINTF("%c=%02x ", regnames[i], REGS8_R(regs,i));
			}
			REGS8_W(oldregs, i, REGS8_R(regs, i));
		}
	}
	for (i=4; i<5; i++) {
		if (REGS16_R(regs, i) != REGS16_R(oldregs, i)) {
			DPRINTF("%s=%04x ", regnamech16[i], REGS16_R(regs, i));
			REGS16_W(oldregs, i, REGS16_R(regs, i));
		}
	}
	DPRINTF("\n");
}
#endif

static void decode_ins(void)
{
	unsigned char op;
	unsigned short pc = REGS16_R(regs, PC);

	REGS16_W(regs, PC, pc + 1);
	op = mem_get(pc);
	DPRINTF("%04x: %02x", pc, op);
	ops[op].fn(op, &ops[op]);
}

static short soundbuf[4096];
static int soundbufpos;
static int sound_rate = 44100;
static int sound_div_tc;
static int sound_div;
static int main_div_tc = 32;
static int main_div;
static int sweep_div_tc = 256;
static int sweep_div;

int r_smpl;
int l_smpl;
int smpldivisor;

int dspfd;

static unsigned int lfsr = 0xffffffff;
static int ch3pos;

static void do_sound(int cycles)
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
			write(dspfd, soundbuf, sizeof(soundbuf));
		}
	}
	if (ch3.master) for (i=0; i<cycles; i++) {
		ch3.div--;
		if (ch3.div <= 0) {
			ch3.div = ch3.div_tc;
			ch3pos++;
		}
	}
	main_div += cycles;
	while (main_div > main_div_tc) {
		main_div -= main_div_tc;

		if (ch1.master) {
			int val = ch1.volume;
			if (ch1.div > ch1.duty_tc) {
				val = -val;
			}
			if (ch1.leftgate) l_smpl += val;
			if (ch1.rightgate) r_smpl += val;
			ch1.div--;
			if (ch1.div <= 0) {
				ch1.div = ch1.div_tc;
			}
		}
		if (ch2.master) {
			int val = ch2.volume;
			if (ch2.div > ch2.duty_tc) {
				val = -val;
			}
			if (ch2.leftgate) l_smpl += val;
			if (ch2.rightgate) r_smpl += val;
			ch2.div--;
			if (ch2.div <= 0) {
				ch2.div = ch2.div_tc;
			}
		}
		if (ch3.master) {
			int val = ((ioregs[0x30 + ((ch3pos >> 2) & 0xf)] >> ((~ch3pos & 2)*2)) & 0xf)*2;
			if (ch3.volume) {
				val = val >> (ch3.volume-1);
			} else val = 0;
//			val = val * ch3.volume/15;
			if (ch3.leftgate) l_smpl += val;
			if (ch3.rightgate) r_smpl += val;
		}
		if (ch4.master) {
//			int val = ch4.volume * (((lfsr >> 13) & 2)-1);
//			int val = ch4.volume * ((random() & 2)-1);
			static int val;
			if (ch4.leftgate) l_smpl += val;
			if (ch4.rightgate) r_smpl += val;
			ch4.div--;
			if (ch4.div <= 0) {
				ch4.div = ch4.div_tc;
				lfsr = (lfsr << 1) | (((lfsr >> 15) ^ (lfsr >> 14)) & 1);
				val = ch4.volume * ((random() & 2)-1);
//				val = ch4.volume * ((lfsr & 2)-1);
			}
		}
		smpldivisor++;

		sweep_div += 1;
		if (sweep_div >= sweep_div_tc) {
			sweep_div = 0;
			if (ch1.sweep_speed_tc) {
				ch1.sweep_speed--;
				if (ch1.sweep_speed < 0) {
					int val = ch1.div_tc >> ch1.sweep_shift;

					ch1.sweep_speed = ch1.sweep_speed_tc;
					if (ch1.sweep_dir) {
						if (ch1.div_tc < 2048 - val) ch1.div_tc += val;
					} else {
						if (ch1.div_tc > val) ch1.div_tc -= val;
					}
					ch1.duty_tc = ch1.div_tc*ch1.duty/8;
				}
			}
			if (ch1.len > 0 && ch1.len_enable) {
				ch1.len--;
				if (ch1.len == 0) ch1.volume = 0;
			}
			if (ch1.env_speed_tc) {
				ch1.env_speed--;
				if (ch1.env_speed <=0) {
					ch1.env_speed = ch1.env_speed_tc;
					if (!ch1.env_dir) {
						if (ch1.volume > 0)
							ch1.volume--;
					} else {
						if (ch1.volume < 15)
							ch1.volume++;
					}
				}
			}
			if (ch2.len > 0 && ch2.len_enable) {
				ch2.len--;
				if (ch2.len == 0) ch2.volume = 0;
			}
			if (ch2.env_speed_tc) {
				ch2.env_speed--;
				if (ch2.env_speed <=0) {
					ch2.env_speed = ch2.env_speed_tc;
					if (!ch2.env_dir) {
						if (ch2.volume > 0)
							ch2.volume--;
					} else {
						if (ch2.volume < 15)
							ch2.volume++;
					}
				}
			}
			if (ch3.len > 0 && ch3.len_enable) {
				ch3.len--;
				if (ch3.len == 0) ch3.volume = 0;
			}
			if (ch3.env_speed_tc) {
				ch3.env_speed--;
				if (ch3.env_speed <=0) {
					ch3.env_speed = ch3.env_speed_tc;
					if (!ch3.env_dir) {
						if (ch3.volume > 0)
							ch3.volume--;
					} else {
						if (ch3.volume < 15)
							ch3.volume++;
					}
				}
			}
			if (ch4.len > 0 && ch4.len_enable) {
				ch4.len--;
				if (ch4.len == 0) ch4.volume = 0;
			}
			if (ch4.env_speed_tc) {
				ch4.env_speed--;
				if (ch4.env_speed <=0) {
					ch4.env_speed = ch4.env_speed_tc;
					if (!ch4.env_dir) {
						if (ch4.volume > 0)
							ch4.volume--;
					} else {
						if (ch4.volume < 15)
							ch4.volume++;
					}
				}
			}
		}
	}
}

static char playercode[] = {
	0x01, 0x30, 0x00,  /* 0050:  ld   bc, 0x0030 */
	0x11, 0x10, 0xff,  /* 0053:  ld   de, 0xff10 */
	0x21, 0x72, 0x00,  /* 0056:  ld   hl, 0x0072 */
	0x2a,              /* 0059:  ldi  a, [hl]    */
	0x12,              /* 005a:  ld   [de], a    */
	0x13,              /* 005b:  inc  de         */
	0x0b,              /* 005c:  dec  bc         */
	0x78,              /* 005d:  ld   a, b       */
	0xb1,              /* 005e:  or   a, c       */
	0x20, 0xf8,        /* 005f:  jr nz $-0x08    */
	0x1e, 0xff,        /* 0061:  ld   e, 0xff    */
	0x3e, 0x06,        /* 0063:  ld   a, 0x06    */
	0x12,              /* 0065:  ld   [de], a    */
	0x3e, 0x00,        /* 0066:  ld   a, 0x00    */
	0xcd, 0x00, 0x00,  /* 0068:  call init       */
	0xfb,              /* 006b:  ei              */
	0x76,              /* 006c:  halt            */
	0xcd, 0x00, 0x00,  /* 006d:  call play       */
	0x18, 0xfa,        /* 0070:  jr $-6          */

	0x80, 0xbf, 0x00, 0x00, 0xbf, /* 0072: initdata */
	0x00, 0x3f, 0x00, 0x00, 0xbf,
	0x7f, 0xff, 0x9f, 0x00, 0xbf,
	0x00, 0xff, 0x00, 0x00, 0xbf,
	0x77, 0xf3, 0xf1, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0xac, 0xdd, 0xda, 0x48,
	0x36, 0x02, 0xcf, 0x16,
	0x2c, 0x04, 0xe5, 0x2c,
	0xac, 0xdd, 0xda, 0x48
};

static void open_gbs(char *name, int i)
{
	int fd, j;
	unsigned char buf[0x70];
	struct stat st;

	if ((fd = open(name, O_RDONLY)) == -1) {
		printf("Could not open %s: %s\n", name, strerror(errno));
		exit(1);
	}
	fstat(fd, &st);
	read(fd, buf, sizeof(buf));
	if (buf[0] != 'G' ||
	    buf[1] != 'B' ||
	    buf[2] != 'S' ||
	    buf[3] != 1) {
		printf("Not a GBS-File: %s\n", name);
		exit(1);
	}

	gbs_base  = buf[0x06] + (buf[0x07] << 8);
	gbs_init  = buf[0x08] + (buf[0x09] << 8);
	gbs_play  = buf[0x0a] + (buf[0x0b] << 8);
	gbs_stack = buf[0x0c] + (buf[0x0d] << 8);

	romsize = (st.st_size + gbs_base + 0x3fff) & ~0x3fff;
	lastbank = (romsize / 0x4000)-1;

	if (i == -1) i = buf[0x5];
	if (i > buf[0x04]) {
		printf("Subsong number out of range (min=1, max=%d).\n", buf[0x4]);
		exit(1);
	}

	printf("Playing song %d/%d.\n"
	       "Title:     \"%.32s\"\n"
	       "Author:    \"%.32s\"\n"
	       "Copyright: \"%.32s\"\n"
	       "Load address %04x.\n"
	       "Init address %04x.\n"
	       "Play address %04x.\n"
	       "Stack pointer %04x.\n"
	       "File size %08lx.\n"
	       "ROM size %08x (%d banks).\n",
	       i, buf[0x04],
	       &buf[0x10],
	       &buf[0x30],
	       &buf[0x50],
	       gbs_base,
	       gbs_init,
	       gbs_play,
	       gbs_stack,
	       st.st_size,
	       romsize,
	       romsize/0x4000);


	if (buf[0x0f] & 4) {
		ioregs[0x06] = buf[0x0e];
		ioregs[0x07] = buf[0x0f];
		timertc = (256-buf[0x0e]) * (16 << (((buf[0x0f]+3) & 3) << 1));
		if ((buf[0x0f] & 0xf0) == 0x80) timertc /= 2;
		printf("Callback rate %2.2fHz (Custom).\n", 4194304/(float)timertc);
	} else {
		timertc = 70256;
		printf("Callback rate %2.2fHz (VBlank).\n", 4194304/(float)timertc);
	}

	rom = malloc(romsize);
	read(fd, &rom[gbs_base], st.st_size - 0x70);

	for (j=0; j<8; j++) {
		int addr = gbs_base + 8*j; /* jump address */
		rom[8*j]   = 0xc3; /* jr imm16 */
		rom[8*j+1] = addr & 0xff;
		rom[8*j+2] = addr >> 8;
	}
	rom[0x40] = 0xc9; /* reti */
	rom[0x48] = 0xc9; /* reti */

	memcpy(&rom[0x50], playercode, sizeof(playercode));

	rom[0x67] = i - 1;
	rom[0x69] = gbs_init & 0xff;
	rom[0x6a] = gbs_init >> 8;
	rom[0x6e] = gbs_play & 0xff;
	rom[0x6f] = gbs_play >> 8;

	REGS16_W(regs, PC, 0x0050); /* playercode entry point */
	REGS16_W(regs, SP, gbs_stack);

	close(fd);
}

static char notes[7] = "CDEFGAH";
static void setnotes(char *s, unsigned int i)
{
	int n = i % 12;

	s[2] = '0' + i / 12;
	n += n > 4;
	s[0] = notes[n >> 1];
	if (n & 1) s[1] = '#';
}

static char vols[4] = " -=#";
static void setvols(char *s, int i)
{
	int j;
	for (j=0; j<4; j++) {
		if (i>=4) {
			s[j] = '%';
			i -= 4;
		} else {
			s[j] = vols[i];
			i = 0;
		}
	}
}

#define LN2 .69314718055994530941
#define MAGIC 6.02980484763069750723
#define FREQ(x) (262144 / x)
// #define NOTE(x) ((log(FREQ(x))/LN2 - log(65.33593)/LN2)*12 + .2)
#define NOTE(x) ((log(FREQ(x))/LN2 - MAGIC)*12 + .2)

static int statustc = 83886;
static int statuscnt;

static char n1[4];
static char n2[4];
static char n3[4];
static char v1[5];
static char v2[5];
static char v3[5];
static char v4[5];

int main(int argc, char **argv)
{
	dspfd = open("/dev/dsp", O_WRONLY);
	int c;
	int subsong = -1;

	c=AFMT_S16_LE;
	ioctl(dspfd, SNDCTL_DSP_SETFMT, &c);
	c=1;
	ioctl(dspfd, SNDCTL_DSP_STEREO, &c);
	c=44100;
	ioctl(dspfd, SNDCTL_DSP_SPEED, &c);
	c=(4 << 16) + 11;
	ioctl(dspfd, SNDCTL_DSP_SETFRAGMENT, &c);
	
	sound_div_tc = (long long)4194304*65536/sound_rate;
	
	ch1.duty = 4;
	ch2.duty = 4;
	ch1.div_tc = ch2.div_tc = ch3.div_tc = 1;

	if (argc < 2) {
		printf("Usage: %s <gbs-file> [<subsong>]\n", argv[0]);
		exit(1);
	}
	if (argc == 3) sscanf(argv[2], "%d", &subsong);
	open_gbs(argv[1], subsong);
	dump_regs();
	oldregs = regs;
	while (1) {
		if (!halted) {
			decode_ins();
			DEB(show_reg_diffs());
		} else cycles = 16;
		clock += cycles;
		statuscnt -= cycles;
		if (timer > 0) timer -= cycles;
		if (timer <= 0 && interrupts && (ioregs[0x7f] & 4)) {
			timer += timertc;
			halted = 0;
			push(REGS16_R(regs, PC));
			REGS16_W(regs, PC, 0x0048); /* timer int handler */
		}
		if (statuscnt < 0) {
			int ni1 = (int)NOTE(ch1.div_tc);
			int ni2 = (int)NOTE(ch2.div_tc);
			int ni3 = (int)NOTE(ch3.div_tc);

			statuscnt += statustc;

			setnotes(n1, ni1);
			setnotes(n2, ni2);
			setnotes(n3, ni3);
			setvols(v1, ch1.volume);
			setvols(v2, ch2.volume);
			setvols(v3, (3-((ch3.volume+3)&3)) << 2);
			setvols(v4, ch4.volume);
			if (!ch1.volume) strncpy(n1, "---", 4);
			if (!ch2.volume) strncpy(n2, "---", 4);
			if (!ch3.volume) strncpy(n3, "---", 4);

			printf("ch1:%s %s ch2:%s %s  ch3:%s %s ch4:%s\r",
				n1, v1, n2, v2, n3, v3, v4);
			fflush(stdout);
		}
		do_sound(cycles);
		cycles = 0;
	}
	return 0;
}
