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

#define ZF	0x80
#define SF	0x40
#define HF	0x20
#define CF	0x10

#define BC 0
#define DE 1
#define HL 2
#define FA 3
#define SP 4
#define PC 5

#define REGS16_R(r, i) ((r.ri[(i)*2]<<8) + r.ri[(i)*2+1])
#define REGS16_W(r, i, x) do { \
	unsigned short v = x; \
	r.ri[(i)*2] = v >> 8; \
	r.ri[(i)*2+1] = v & 0xff; \
} while (0)

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
			unsigned short sp_dontuse;
			unsigned short pc_dontuse;
		} rn;
	};
} regs;

static struct regs oldregs;

struct opinfo;

typedef void (*ex_fn)(unsigned char op, struct opinfo *oi);
typedef void (*put_fn)(unsigned short addr, unsigned char val);
typedef unsigned char (*get_fn)(unsigned short addr);

struct opinfo {
	char *name;
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
	} else if (addr >= 0xff10 &&
	           addr <= 0xff3f) {
		return ioregs[addr & 0x7f];
	}
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

static unsigned char mem_get(unsigned short addr)
{
	unsigned char res = getlookup[addr >> 8](addr);
	cycles++;
	return res;
}

static void vidram_put(unsigned short addr, unsigned char val)
{
}

static void rom_put(unsigned short addr, unsigned char val)
{
	if (addr >= 0x2000 && addr <= 0x3fff)
		rombank = val;
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

					ch1.div = 0;
					ch1.div_tc = 2048 - div;
					ch1.duty = dutylookup[duty];
					ch1.duty_tc = ch1.div_tc*ch1.duty/8;
					ch1.len = (64 - len)*2;
					ch1.len_enable = (val & 0x40) > 0;

//					printf(" ch1: vol=%02d envd=%d envspd=%d duty=%d len=%02d len_en=%d key=%04d \n", ch1.volume, ch1.env_dir, ch1.env_speed_tc, ch1.duty, ch1.len, ch1.len_enable, ch1.div_tc);
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
					ch1.div = 0;
					ch2.div_tc = 2048 - div;
					ch2.duty = dutylookup[duty];
					ch2.duty_tc = ch2.div_tc*ch2.duty/8;
					ch2.len = (64 - len)*2;
					ch2.len_enable = (val & 0x40) > 0;

//					printf(" ch2: vol=%02d envd=%d envspd=%d duty=%d len=%02d len_en=%d key=%04d \n", ch2.volume, ch2.env_dir, ch2.env_speed, ch2.duty, ch2.len, ch2.len_enable, ch2.div_tc);
				}
				break;
			case 0xff1a:
				ch3.master = (ioregs[0x1a] & 0x80) > 0;
				break;
			case 0xff1b:
				{
					int duty = ioregs[0x1b] >> 6;
					int len = ioregs[0x1b] & 0x3f;

					ch3.duty = dutylookup[duty];
					ch3.duty_tc = ch3.div_tc*ch3.duty/8;
					ch3.len = (64 - len)*2;

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
					int len = ioregs[0x1b] & 0x3f;
					int div = ioregs[0x1d];
					div |= ((int)val & 7) << 8;
					ch3.master = (ioregs[0x1a] & 0x80) > 0;
					ch3.volume = vol;
					ch1.div = 0;
					ch3.div_tc = 2048 - div;
					ch3.len = (64 - len)*2;
					ch3.len_enable = (val & 0x40) > 0;
//					printf(" ch3: sft=%02d envd=%d envspd=%d duty=%d len=%02d len_en=%d key=%04d \n", ch3.volume, ch3.env_dir, ch3.env_speed, ch3.duty, ch3.len, ch3.len_enable, ch3.div_tc);
				}
				break;
			case 0xff1f:
				break;
			case 0xff20:
				{
					int len = ioregs[0x20] & 0x3f;

					ch3.len = (64 - len)*2;

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
				{
					int div = ioregs[0x22];
					div |= ((int)val & 7) << 8;
					ch4.div = 0;
					ch4.div_tc = 1 << (div >> 5);
					if (div & 7) ch4.div_tc *= div & 7;
					else ch4.div_tc /= 2;
				}
				break;
			case 0xff23:
				if (val & 0x80) {
					int vol = ioregs[0x21] >> 4;
					int envdir = (ioregs[0x21] >> 3) & 1;
					int envspd = ioregs[0x21] & 7;
					int len = ioregs[0x20] & 0x3f;
					int div = ioregs[0x22];

					div |= ((int)val & 7) << 8;
					ch4.volume = vol;
					ch4.master = 1;
					ch4.env_dir = envdir;
					ch4.env_speed = ch4.env_speed_tc = envspd*8;
					ch4.len = (64 - len)*2;
					ch4.len_enable = (val & 0x40) > 0;

//					printf(" ch4: vol=%02d envd=%d envspd=%d duty=%d len=%02d len_en=%d key=%04d \n", ch4.volume, ch4.env_dir, ch4.env_speed, ch4.duty, ch4.len, ch4.len_enable, ch4.div_tc);
				}
				break;
			case 0xff24:
				break;
			case 0xff25:
				ch1.leftgate = val & 0x10;
				ch1.rightgate = val & 0x01;
				ch2.leftgate = val & 0x20;
				ch2.rightgate = val & 0x02;
				ch3.leftgate = val & 0x40;
				ch3.rightgate = val & 0x04;
				ch4.leftgate = val & 0x80;
				ch4.rightgate = val & 0x08;
				break;
			case 0xff26:
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
			default:
				printf("iowrite to 0x%04x unimplemented.\n", addr);
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

static void mem_put(unsigned short addr, unsigned char val)
{
	cycles++;
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

static void print_reg(int i)
{
	if (i == 6) DPRINTF("[HL]"); /* indirect memory access by [HL] */
	else DPRINTF("%c", regnames[i]);
}

static unsigned char get_reg(int i)
{
	if (i == 6) /* indirect memory access by [HL] */
		return mem_get(REGS16_R(regs, HL));
	return regs.ri[i];
}

static void put_reg(int i, unsigned char val)
{
	if (i == 6) /* indirect memory access by [HL] */
		mem_put(REGS16_R(regs, HL), val);
	else regs.ri[i] = val;
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

	DPRINTF(" SET %d,", bit);
	print_reg(reg);
	put_reg(reg, get_reg(reg) | (1 << bit));
}

static void op_res(unsigned char op)
{
	int reg = op & 7;
	int bit = (op >> 3) & 7;

	DPRINTF(" RES %d,", bit);
	print_reg(reg);
	put_reg(reg, get_reg(reg) & ~(1 << bit));
}

static void op_bit(unsigned char op)
{
	int reg = op & 7;
	int bit = (op >> 3) & 7;
	unsigned char res;

	DPRINTF(" BIT %d,", bit);
	print_reg(reg);
	res = get_reg(reg) & (1 << bit);
	if (res) regs.rn.f &= ~ZF; else regs.rn.f |= ZF;
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
	res |= (((unsigned short)regs.rn.f & CF) >> 4);
	regs.rn.f &= ~CF;
	regs.rn.f |= (val >> 7) << 4;
	if (res) regs.rn.f &= ~ZF; else regs.rn.f |= ZF;
	put_reg(reg, res);
}

static void op_rlca(unsigned char op, struct opinfo *oi)
{
	unsigned short res;

	DPRINTF(" %s", oi->name);
	res  = regs.rn.a;
	res  = res << 1;
	res |= (((unsigned short)regs.rn.f & CF) >> 4);
	regs.rn.f &= ~CF;
	regs.rn.f |= (regs.rn.a >> 7) << 4;
	if (res) regs.rn.f &= ~ZF; else regs.rn.f |= ZF;
	regs.rn.a = res;
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
	res |= (val >> 7);
	regs.rn.f &= ~CF;
	regs.rn.f |= (val >> 7) << 4;
	if (res) regs.rn.f &= ~ZF; else regs.rn.f |= ZF;
	put_reg(reg, res);
}

static void op_rla(unsigned char op, struct opinfo *oi)
{
	unsigned short res;

	DPRINTF(" %s", oi->name);
	res  = regs.rn.a;
	res  = res << 1;
	res |= (regs.rn.a >> 7);
	regs.rn.f &= ~CF;
	regs.rn.f |= (regs.rn.a >> 7) << 4;
	if (res) regs.rn.f &= ~ZF; else regs.rn.f |= ZF;
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
	regs.rn.f &= ~CF;
	regs.rn.f |= ((val >> 7) << 4);
	if (res) regs.rn.f &= ~ZF; else regs.rn.f |= ZF;
	put_reg(reg, res);
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
	res |= (((unsigned short)regs.rn.f & CF) << 3);
	regs.rn.f &= ~CF;
	regs.rn.f |= (val & 1) << 4;
	if (res) regs.rn.f &= ~ZF; else regs.rn.f |= ZF;
	put_reg(reg, res);
}

static void op_rrca(unsigned char op, struct opinfo *oi)
{
	unsigned short res;

	DPRINTF(" %s", oi->name);
	res  = regs.rn.a;
	res  = res >> 1;
	res |= (((unsigned short)regs.rn.f & CF) << 3);
	regs.rn.f &= ~CF;
	regs.rn.f |= (regs.rn.a & 1) << 4;
	if (res) regs.rn.f &= ~ZF; else regs.rn.f |= ZF;
	regs.rn.a = res;
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
	res |= (val << 7);
	regs.rn.f &= ~CF;
	regs.rn.f |= (val & 1) << 4;
	if (res) regs.rn.f &= ~ZF; else regs.rn.f |= ZF;
	put_reg(reg, res);
}

static void op_rra(unsigned char op, struct opinfo *oi)
{
	unsigned short res;

	DPRINTF(" %s", oi->name);
	res  = regs.rn.a;
	res  = res >> 1;
	res |= (regs.rn.a << 7);
	regs.rn.f &= ~CF;
	regs.rn.f |= (regs.rn.a & 1) << 4;
	if (res) regs.rn.f &= ~ZF; else regs.rn.f |= ZF;
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
	regs.rn.f &= ~CF;
	regs.rn.f |= ((val & 1) << 4);
	if (res) regs.rn.f &= ~ZF; else regs.rn.f |= ZF;
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
	regs.rn.f &= ~CF;
	regs.rn.f |= ((val & 1) << 4);
	if (res) regs.rn.f &= ~ZF; else regs.rn.f |= ZF;
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
	if (res) regs.rn.f &= ~ZF; else regs.rn.f |= ZF;
	put_reg(reg, res);
}

static struct opinfo cbops[8] = {
	{"RLC", &op_rlc},		/* opcode cb00-cb07 */
	{"RRC", &op_rrc},		/* opcode cb08-cb0f */
	{"RL", &op_rl},		/* opcode cb10-cb17 */
	{"RR", &op_rr},		/* opcode cb18-cb1f */
	{"SLA", &op_sla},		/* opcode cb20-cb27 */
	{"SRA", &op_sra},		/* opcode cb28-cb2f */
	{"SWAP", &op_swap},		/* opcode cb30-cb37 */
	{"SRL", &op_srl},		/* opcode cb38-cb3f */
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

	if (dst == 6) { /* indirect memory access by [HL] */
		DPRINTF(" %s [HL],", oi->name);
		print_reg(src);
		mem_put(REGS16_R(regs, HL), get_reg(src));
	} else {
		DPRINTF(" %s %c,", oi->name, regnames[dst]);
		print_reg(src);
		regs.ri[dst] = get_reg(src);
	}
}

static void op_ld_imm(unsigned char op, struct opinfo *oi)
{
	int ofs = get_imm16();

	DPRINTF(" %s A, [0x%04x] ", oi->name, ofs);
	regs.rn.a = mem_get(ofs);
}

static void op_ld_ind16_a(unsigned char op, struct opinfo *oi)
{
	int ofs = get_imm16();

	DPRINTF(" %s [0x%04x], A", oi->name, ofs);
	mem_put(ofs, regs.rn.a);
}

static void op_ld_ind16_sp(unsigned char op, struct opinfo *oi)
{
	int ofs = get_imm16();
	int sp = REGS16_R(regs, SP);

	DPRINTF(" %s [0x%04x], A", oi->name, ofs);
	mem_put(ofs, sp & 0xff);
	mem_put(ofs+1, sp >> 8);
}

static void op_ld_reg16_imm(unsigned char op, struct opinfo *oi)
{
	int val = get_imm16();
	int reg = (op >> 4) & 3;

	reg += reg > 2; /* skip over FA */
	DPRINTF(" %s %s, 0x%02x", oi->name, regnamech16[reg], val);
	REGS16_W(regs, reg, val);
}

static void op_ld_reg16_a(unsigned char op, struct opinfo *oi)
{
	int reg = (op >> 4) & 3;
	unsigned short r;

	reg -= reg > 2;
	if (op & 8) {
		DPRINTF(" %s A, [%s]", oi->name, regnamech16[reg]);
		regs.rn.a = mem_get(r = REGS16_R(regs, reg));
	} else {
		DPRINTF(" %s [%s], A", oi->name, regnamech16[reg]);
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

	DPRINTF(" %s ", oi->name);
	print_reg(reg);
	put_reg(reg, val);
	DPRINTF(",%02x", val);
}

static void op_ldh(unsigned char op, struct opinfo *oi)
{
	int ofs = op & 2 ? 0 : get_imm8();

	if (op & 0x10) {
		DPRINTF(" %s A,", oi->name);
		if ((op & 2) == 0) {
			DPRINTF("[%02x]", ofs);
		} else {
			ofs = regs.rn.c;
			DPRINTF("[C]");
		}
		regs.rn.a = mem_get(0xff00 + ofs);
	} else {
		if ((op & 2) == 0) {
			DPRINTF(" %s [%02x], A", oi->name, ofs);
		} else {
			ofs = regs.rn.c;
			DPRINTF(" %s [C], A", oi->name);
		}
		mem_put(0xff00 + ofs, regs.rn.a);
	}
}

static void op_inc(unsigned char op, struct opinfo *oi)
{
	int reg = (op >> 3) & 7;
	unsigned char res;

	if (reg != 6) {
		DPRINTF(" %s %c", oi->name, regnames[reg]);
		res = ++regs.ri[reg];
	} else {
		unsigned short hl = REGS16_R(regs, HL);

		res = mem_get(hl) + 1;
		DPRINTF(" %s [HL]", oi->name);
		mem_put(hl, res);
	}
	if (res == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_inc16(unsigned char op, struct opinfo *oi)
{
	int reg = (op >> 4) & 3;
	unsigned short res = REGS16_R(regs, reg);

	DPRINTF(" %s %s", oi->name, regnamech16[reg]);
	res++;
	REGS16_W(regs, reg, res);
//	if (res == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_dec(unsigned char op, struct opinfo *oi)
{
	int reg = (op >> 3) & 7;
	unsigned char res;

	if (reg != 6) {
		DPRINTF(" %s %c", oi->name, regnames[reg]);
		res = --regs.ri[reg];
	} else {
		unsigned short hl = REGS16_R(regs, HL);

		res = mem_get(hl) - 1;
		DPRINTF(" %s [HL]", oi->name);
		mem_put(hl, res);
	}
	if (res == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_dec16(unsigned char op, struct opinfo *oi)
{
	int reg = (op >> 4) & 3;
	unsigned short res = REGS16_R(regs, reg);

	DPRINTF(" %s %s", oi->name, regnamech16[reg]);
	res--;
	REGS16_W(regs, reg, res);
//	if (res == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_add_sp_imm(unsigned char op, struct opinfo *oi)
{
	unsigned char imm = get_imm8();
	unsigned short old = REGS16_R(regs, SP);
	unsigned short new = old;

	DPRINTF(" %s SP, %02x", oi->name, imm);
	new += imm;
	REGS16_W(regs, SP, new);
	if (old > new) regs.rn.f |= CF; else regs.rn.f &= ~CF;
	if ((old & 0xfff) > (new & 0xfff)) regs.rn.f |= HF; else regs.rn.f &= ~HF;
	if (new == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_add(unsigned char op, struct opinfo *oi)
{
	unsigned char old = regs.rn.a;
	unsigned char new;

	DPRINTF(" %s A,", oi->name);
	print_reg(op & 7);
	regs.rn.a += get_reg(op & 7);
	regs.rn.f &= ~SF;
	new = regs.rn.a;
	if (old > new) regs.rn.f |= CF; else regs.rn.f &= ~CF;
	if ((old & 15) > (new & 15)) regs.rn.f |= HF; else regs.rn.f &= ~HF;
	if (new == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_add_imm(unsigned char op, struct opinfo *oi)
{
	unsigned char imm = get_imm8();
	unsigned char old = regs.rn.a;
	unsigned char new = old;

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	new += imm;
	regs.rn.f &= ~SF;
	regs.rn.a = new;
	if (old > new) regs.rn.f |= CF; else regs.rn.f &= ~CF;
	if ((old & 15) > (new & 15)) regs.rn.f |= HF; else regs.rn.f &= ~HF;
	if (new == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
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

	regs.rn.f &= ~SF;
	if (old > new) regs.rn.f |= CF; else regs.rn.f &= ~CF;
	if ((old & 0xfff) > (new & 0xfff)) regs.rn.f |= HF; else regs.rn.f &= ~HF;
}

static void op_adc(unsigned char op, struct opinfo *oi)
{
	unsigned char old = regs.rn.a;
	unsigned char new;

	DPRINTF(" %s A,", oi->name);
	print_reg(op & 7);
	regs.rn.a += get_reg(op & 7);
	regs.rn.a += (regs.rn.f & CF) > 0;
	regs.rn.f &= ~SF;
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
	regs.rn.f &= ~SF;
	regs.rn.a = new;
	if (old > new) regs.rn.f |= CF; else regs.rn.f &= ~CF;
	if ((old & 15) > (new & 15)) regs.rn.f |= HF; else regs.rn.f &= ~HF;
	if (new == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_cp(unsigned char op, struct opinfo *oi)
{
	unsigned char old = regs.rn.a;
	unsigned char new = old;

	DPRINTF(" %s A,", oi->name);
	print_reg(op & 7);
	new -= get_reg(op & 7);
	if (old < new) regs.rn.f |= CF; else regs.rn.f &= ~CF;
	if ((old & 15) < (new & 15)) regs.rn.f |= HF; else regs.rn.f &= ~HF;
	if (new == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_cp_imm(unsigned char op, struct opinfo *oi)
{
	unsigned char imm = get_imm8();
	unsigned char old = regs.rn.a;
	unsigned char new = old;

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	new -= imm;
	if (old < new) regs.rn.f |= CF; else regs.rn.f &= ~CF;
	if ((old & 15) < (new & 15)) regs.rn.f |= HF; else regs.rn.f &= ~HF;
	if (new == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_sub(unsigned char op, struct opinfo *oi)
{
	unsigned char old = regs.rn.a;
	unsigned char new;

	DPRINTF(" %s A,", oi->name);
	print_reg(op & 7);
	regs.rn.a -= get_reg(op & 7);
	regs.rn.f |= SF;
	new = regs.rn.a;
	if (old < new) regs.rn.f |= CF; else regs.rn.f &= ~CF;
	if ((old & 15) < (new & 15)) regs.rn.f |= HF; else regs.rn.f &= ~HF;
	if (new == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_sub_imm(unsigned char op, struct opinfo *oi)
{
	unsigned char imm = get_imm8();
	unsigned char old = regs.rn.a;
	unsigned char new = old;

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	new -= imm;
	regs.rn.f |= SF;
	regs.rn.a = new;
	if (old < new) regs.rn.f |= CF; else regs.rn.f &= ~CF;
	if ((old & 15) < (new & 15)) regs.rn.f |= HF; else regs.rn.f &= ~HF;
	if (new == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_sbc(unsigned char op, struct opinfo *oi)
{
	unsigned char old = regs.rn.a;
	unsigned char new;

	DPRINTF(" %s A,", oi->name);
	print_reg(op & 7);
	regs.rn.a -= get_reg(op & 7);
	regs.rn.a -= (regs.rn.f & CF) > 0;
	regs.rn.f |= SF;
	new = regs.rn.a;
	if (old < new) regs.rn.f |= CF; else regs.rn.f &= ~CF;
	if ((old & 15) < (new & 15)) regs.rn.f |= HF; else regs.rn.f &= ~HF;
	if (new == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_sbc_imm(unsigned char op, struct opinfo *oi)
{
	unsigned char imm = get_imm8();
	unsigned char old = regs.rn.a;
	unsigned char new = old;

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	new -= imm;
	new -= (regs.rn.f & CF) > 0;
	regs.rn.f |= SF;
	regs.rn.a = new;
	if (old < new) regs.rn.f |= CF; else regs.rn.f &= ~CF;
	if ((old & 15) < (new & 15)) regs.rn.f |= HF; else regs.rn.f &= ~HF;
	if (new == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_and(unsigned char op, struct opinfo *oi)
{
	DPRINTF(" %s A,", oi->name);
	print_reg(op & 7);
	regs.rn.a &= get_reg(op & 7);
	if (regs.rn.a == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_and_imm(unsigned char op, struct opinfo *oi)
{
	unsigned char imm = get_imm8();

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	regs.rn.a &= imm;
	if (regs.rn.a == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_or(unsigned char op, struct opinfo *oi)
{
	DPRINTF(" %s A,", oi->name);
	print_reg(op & 7);
	regs.rn.a |= get_reg(op & 7);
	if (regs.rn.a == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_or_imm(unsigned char op, struct opinfo *oi)
{
	unsigned char imm = get_imm8();

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	regs.rn.a |= imm;
	if (regs.rn.a == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_xor(unsigned char op, struct opinfo *oi)
{
	DPRINTF(" %s A,", oi->name);
	print_reg(op & 7);
	regs.rn.a ^= get_reg(op & 7);
	if (regs.rn.a == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_xor_imm(unsigned char op, struct opinfo *oi)
{
	unsigned char imm = get_imm8();

	DPRINTF(" %s A, $0x%02x", oi->name, imm);
	regs.rn.a ^= imm;
	if (regs.rn.a == 0) regs.rn.f |= ZF; else regs.rn.f &= ~ZF;
}

static void op_push(unsigned char op, struct opinfo *oi)
{
	int reg = op >> 4 & 3;

	push(REGS16_R(regs, reg));
	DPRINTF(" %s %s", oi->name, regnamech16[reg]);
}

static void op_pop(unsigned char op, struct opinfo *oi)
{
	int reg = op >> 4 & 3;

	REGS16_W(regs, reg, pop());
	DPRINTF(" %s %s", oi->name, regnamech16[reg]);
}

static void op_cpl(unsigned char op, struct opinfo *oi)
{
	DPRINTF(" %s", oi->name);
	regs.rn.a = ~regs.rn.a;
}

static void op_ccf(unsigned char op, struct opinfo *oi)
{
	DPRINTF(" %s", oi->name);
	regs.rn.f ^= CF;
}

static void op_scf(unsigned char op, struct opinfo *oi)
{
	DPRINTF(" %s", oi->name);
	regs.rn.f |= CF;
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
	REGS16_W(regs, PC, gbs_base + ofs);
}

static void op_nop(unsigned char op, struct opinfo *oi)
{
	DPRINTF(" %s", oi->name);
}

static struct opinfo ops[256] = {
	{"NOP", &op_nop},		/* opcode 00 */
	{"LD", &op_ld_reg16_imm},		/* opcode 01 */
	{"LD", &op_ld_reg16_a},		/* opcode 02 */
	{"INC", &op_inc16},		/* opcode 03 */
	{"INC", &op_inc},		/* opcode 04 */
	{"DEC", &op_dec},		/* opcode 05 */
	{"LD", &op_ld_reg8_imm},		/* opcode 06 */
	{"RLCA", &op_rlca},		/* opcode 07 */
	{"LD", &op_ld_ind16_sp},		/* opcode 08 */
	{"ADD", &op_add_hl},		/* opcode 09 */
	{"LD", &op_ld_reg16_a},		/* opcode 0a */
	{"DEC", &op_dec16},		/* opcode 0b */
	{"INC", &op_inc},		/* opcode 0c */
	{"DEC", &op_dec},		/* opcode 0d */
	{"LD", &op_ld_reg8_imm},		/* opcode 0e */
	{"RRCA", &op_rrca},		/* opcode 0f */
	{"STOP", &op_stop},		/* opcode 10 */
	{"LD", &op_ld_reg16_imm},		/* opcode 11 */
	{"LD", &op_ld_reg16_a},		/* opcode 12 */
	{"INC", &op_inc16},		/* opcode 13 */
	{"INC", &op_inc},		/* opcode 14 */
	{"DEC", &op_dec},		/* opcode 15 */
	{"LD", &op_ld_reg8_imm},		/* opcode 16 */
	{"RLA", &op_rla},		/* opcode 17 */
	{"JR", &op_jr},		/* opcode 18 */
	{"ADD", &op_add_hl},		/* opcode 19 */
	{"LD", &op_ld_reg16_a},		/* opcode 1a */
	{"DEC", &op_dec16},		/* opcode 1b */
	{"INC", &op_inc},		/* opcode 1c */
	{"DEC", &op_dec},		/* opcode 1d */
	{"LD", &op_ld_reg8_imm},		/* opcode 1e */
	{"RRA", &op_rra},		/* opcode 1f */
	{"JR", &op_jr_cond},		/* opcode 20 */
	{"LD", &op_ld_reg16_imm},		/* opcode 21 */
	{"LDI", &op_ld_reg16_a},		/* opcode 22 */
	{"INC", &op_inc16},		/* opcode 23 */
	{"INC", &op_inc},		/* opcode 24 */
	{"DEC", &op_dec},		/* opcode 25 */
	{"LD", &op_ld_reg8_imm},		/* opcode 26 */
	{"UNKN", &op_unknown},		/* opcode 27 */
	{"JR", &op_jr_cond},		/* opcode 28 */
	{"ADD", &op_add_hl},		/* opcode 29 */
	{"LDI", &op_ld_reg16_a},		/* opcode 2a */
	{"DEC", &op_dec16},		/* opcode 2b */
	{"INC", &op_inc},		/* opcode 2c */
	{"DEC", &op_dec},		/* opcode 2d */
	{"LD", &op_ld_reg8_imm},		/* opcode 2e */
	{"CPL", &op_cpl},		/* opcode 2f */
	{"JR", &op_jr_cond},		/* opcode 30 */
	{"LD", &op_ld_reg16_imm},		/* opcode 31 */
	{"LDD", &op_ld_reg16_a},		/* opcode 32 */
	{"INC", &op_inc16},		/* opcode 33 */
	{"INC", &op_inc},		/* opcode 34 */
	{"DEC", &op_dec},		/* opcode 35 */
	{"LD", &op_ld_reg8_imm},		/* opcode 36 */
	{"SCF", &op_scf},		/* opcode 37 */
	{"JR", &op_jr_cond},		/* opcode 38 */
	{"ADD", &op_add_hl},		/* opcode 39 */
	{"LDD", &op_ld_reg16_a},		/* opcode 3a */
	{"DEC", &op_dec16},		/* opcode 3b */
	{"INC", &op_inc},		/* opcode 3c */
	{"DEC", &op_dec},		/* opcode 3d */
	{"LD", &op_ld_reg8_imm},		/* opcode 3e */
	{"CCF", &op_ccf},		/* opcode 3f */
	{"LD", &op_ld},		/* opcode 40 */
	{"LD", &op_ld},		/* opcode 41 */
	{"LD", &op_ld},		/* opcode 42 */
	{"LD", &op_ld},		/* opcode 43 */
	{"LD", &op_ld},		/* opcode 44 */
	{"LD", &op_ld},		/* opcode 45 */
	{"LD", &op_ld},		/* opcode 46 */
	{"LD", &op_ld},		/* opcode 47 */
	{"LD", &op_ld},		/* opcode 48 */
	{"LD", &op_ld},		/* opcode 49 */
	{"LD", &op_ld},		/* opcode 4a */
	{"LD", &op_ld},		/* opcode 4b */
	{"LD", &op_ld},		/* opcode 4c */
	{"LD", &op_ld},		/* opcode 4d */
	{"LD", &op_ld},		/* opcode 4e */
	{"LD", &op_ld},		/* opcode 4f */
	{"LD", &op_ld},		/* opcode 50 */
	{"LD", &op_ld},		/* opcode 51 */
	{"LD", &op_ld},		/* opcode 52 */
	{"LD", &op_ld},		/* opcode 53 */
	{"LD", &op_ld},		/* opcode 54 */
	{"LD", &op_ld},		/* opcode 55 */
	{"LD", &op_ld},		/* opcode 56 */
	{"LD", &op_ld},		/* opcode 57 */
	{"LD", &op_ld},		/* opcode 58 */
	{"LD", &op_ld},		/* opcode 59 */
	{"LD", &op_ld},		/* opcode 5a */
	{"LD", &op_ld},		/* opcode 5b */
	{"LD", &op_ld},		/* opcode 5c */
	{"LD", &op_ld},		/* opcode 5d */
	{"LD", &op_ld},		/* opcode 5e */
	{"LD", &op_ld},		/* opcode 5f */
	{"LD", &op_ld},		/* opcode 60 */
	{"LD", &op_ld},		/* opcode 61 */
	{"LD", &op_ld},		/* opcode 62 */
	{"LD", &op_ld},		/* opcode 63 */
	{"LD", &op_ld},		/* opcode 64 */
	{"LD", &op_ld},		/* opcode 65 */
	{"LD", &op_ld},		/* opcode 66 */
	{"LD", &op_ld},		/* opcode 67 */
	{"LD", &op_ld},		/* opcode 68 */
	{"LD", &op_ld},		/* opcode 69 */
	{"LD", &op_ld},		/* opcode 6a */
	{"LD", &op_ld},		/* opcode 6b */
	{"LD", &op_ld},		/* opcode 6c */
	{"LD", &op_ld},		/* opcode 6d */
	{"LD", &op_ld},		/* opcode 6e */
	{"LD", &op_ld},		/* opcode 6f */
	{"LD", &op_ld},		/* opcode 70 */
	{"LD", &op_ld},		/* opcode 71 */
	{"LD", &op_ld},		/* opcode 72 */
	{"LD", &op_ld},		/* opcode 73 */
	{"LD", &op_ld},		/* opcode 74 */
	{"LD", &op_ld},		/* opcode 75 */
	{"HALT", &op_halt},		/* opcode 76 */
	{"LD", &op_ld},		/* opcode 77 */
	{"LD", &op_ld},		/* opcode 78 */
	{"LD", &op_ld},		/* opcode 79 */
	{"LD", &op_ld},		/* opcode 7a */
	{"LD", &op_ld},		/* opcode 7b */
	{"LD", &op_ld},		/* opcode 7c */
	{"LD", &op_ld},		/* opcode 7d */
	{"LD", &op_ld},		/* opcode 7e */
	{"LD", &op_ld},		/* opcode 7f */
	{"ADD", &op_add},		/* opcode 80 */
	{"ADD", &op_add},		/* opcode 81 */
	{"ADD", &op_add},		/* opcode 82 */
	{"ADD", &op_add},		/* opcode 83 */
	{"ADD", &op_add},		/* opcode 84 */
	{"ADD", &op_add},		/* opcode 85 */
	{"ADD", &op_add},		/* opcode 86 */
	{"ADD", &op_add},		/* opcode 87 */
	{"ADC", &op_adc},		/* opcode 88 */
	{"ADC", &op_adc},		/* opcode 89 */
	{"ADC", &op_adc},		/* opcode 8a */
	{"ADC", &op_adc},		/* opcode 8b */
	{"ADC", &op_adc},		/* opcode 8c */
	{"ADC", &op_adc},		/* opcode 8d */
	{"ADC", &op_adc},		/* opcode 8e */
	{"ADC", &op_adc},		/* opcode 8f */
	{"SUB", &op_sub},		/* opcode 90 */
	{"SUB", &op_sub},		/* opcode 91 */
	{"SUB", &op_sub},		/* opcode 92 */
	{"SUB", &op_sub},		/* opcode 93 */
	{"SUB", &op_sub},		/* opcode 94 */
	{"SUB", &op_sub},		/* opcode 95 */
	{"SUB", &op_sub},		/* opcode 96 */
	{"SUB", &op_sub},		/* opcode 97 */
	{"SBC", &op_sbc},		/* opcode 98 */
	{"SBC", &op_sbc},		/* opcode 99 */
	{"SBC", &op_sbc},		/* opcode 9a */
	{"SBC", &op_sbc},		/* opcode 9b */
	{"SBC", &op_sbc},		/* opcode 9c */
	{"SBC", &op_sbc},		/* opcode 9d */
	{"SBC", &op_sbc},		/* opcode 9e */
	{"SBC", &op_sbc},		/* opcode 9f */
	{"AND", &op_and},		/* opcode a0 */
	{"AND", &op_and},		/* opcode a1 */
	{"AND", &op_and},		/* opcode a2 */
	{"AND", &op_and},		/* opcode a3 */
	{"AND", &op_and},		/* opcode a4 */
	{"AND", &op_and},		/* opcode a5 */
	{"AND", &op_and},		/* opcode a6 */
	{"AND", &op_and},		/* opcode a7 */
	{"XOR", &op_xor},		/* opcode a8 */
	{"XOR", &op_xor},		/* opcode a9 */
	{"XOR", &op_xor},		/* opcode aa */
	{"XOR", &op_xor},		/* opcode ab */
	{"XOR", &op_xor},		/* opcode ac */
	{"XOR", &op_xor},		/* opcode ad */
	{"XOR", &op_xor},		/* opcode ae */
	{"XOR", &op_xor},		/* opcode af */
	{"OR", &op_or},		/* opcode b0 */
	{"OR", &op_or},		/* opcode b1 */
	{"OR", &op_or},		/* opcode b2 */
	{"OR", &op_or},		/* opcode b3 */
	{"OR", &op_or},		/* opcode b4 */
	{"OR", &op_or},		/* opcode b5 */
	{"OR", &op_or},		/* opcode b6 */
	{"OR", &op_or},		/* opcode b7 */
	{"CP", &op_cp},		/* opcode b8 */
	{"CP", &op_cp},		/* opcode b9 */
	{"CP", &op_cp},		/* opcode ba */
	{"CP", &op_cp},		/* opcode bb */
	{"CP", &op_cp},		/* opcode bc */
	{"CP", &op_cp},		/* opcode bd */
	{"CP", &op_cp},		/* opcode be */
	{"UNKN", &op_unknown},		/* opcode bf */
	{"RET", &op_ret_cond},		/* opcode c0 */
	{"POP", &op_pop},		/* opcode c1 */
	{"JP", &op_jp_cond},		/* opcode c2 */
	{"JP", &op_jp},		/* opcode c3 */
	{"CALL", &op_call_cond},		/* opcode c4 */
	{"PUSH", &op_push},		/* opcode c5 */
	{"ADD", &op_add_imm},		/* opcode c6 */
	{"RST", &op_rst},		/* opcode c7 */
	{"RET", &op_ret_cond},		/* opcode c8 */
	{"RET", &op_ret},		/* opcode c9 */
	{"JP", &op_jp_cond},		/* opcode ca */
	{"CBPREFIX", &op_cbprefix},		/* opcode cb */
	{"CALL", &op_call_cond},		/* opcode cc */
	{"CALL", &op_call},		/* opcode cd */
	{"ADC", &op_adc_imm},		/* opcode ce */
	{"RST", &op_rst},		/* opcode cf */
	{"RET", &op_ret_cond},		/* opcode d0 */
	{"POP", &op_pop},		/* opcode d1 */
	{"JP", &op_jp_cond},		/* opcode d2 */
	{"UNKN", &op_unknown},		/* opcode d3 */
	{"CALL", &op_call_cond},		/* opcode d4 */
	{"PUSH", &op_push},		/* opcode d5 */
	{"SUB", &op_sub_imm},		/* opcode d6 */
	{"RST", &op_rst},		/* opcode d7 */
	{"RET", &op_ret_cond},		/* opcode d8 */
	{"RETI", &op_reti},		/* opcode d9 */
	{"JP", &op_jp_cond},		/* opcode da */
	{"UNKN", &op_unknown},		/* opcode db */
	{"CALL", &op_call_cond},		/* opcode dc */
	{"UNKN", &op_unknown},		/* opcode dd */
	{"SBC", &op_sbc_imm},		/* opcode de */
	{"RST", &op_rst},		/* opcode df */
	{"LDH", &op_ldh},		/* opcode e0 */
	{"POP", &op_pop},		/* opcode e1 */
	{"LDH", &op_ldh},		/* opcode e2 */
	{"UNKN", &op_unknown},		/* opcode e3 */
	{"UNKN", &op_unknown},		/* opcode e4 */
	{"PUSH", &op_push},		/* opcode e5 */
	{"AND", &op_and_imm},		/* opcode e6 */
	{"RST", &op_rst},		/* opcode e7 */
	{"ADD", &op_add_sp_imm},		/* opcode e8 */
	{"JP", &op_jp_hl},		/* opcode e9 */
	{"LD", &op_ld_ind16_a},		/* opcode ea */
	{"UNKN", &op_unknown},		/* opcode eb */
	{"UNKN", &op_unknown},		/* opcode ec */
	{"UNKN", &op_unknown},		/* opcode ed */
	{"XOR", &op_xor_imm},		/* opcode ee */
	{"RST", &op_rst},		/* opcode ef */
	{"LDH", &op_ldh},		/* opcode f0 */
	{"POP", &op_pop},		/* opcode f1 */
	{"LDH", &op_ldh},		/* opcode f2 */
	{"DI", &op_di},		/* opcode f3 */
	{"UNKN", &op_unknown},		/* opcode f4 */
	{"PUSH", &op_push},		/* opcode f5 */
	{"OR", &op_or_imm},		/* opcode f6 */
	{"RST", &op_rst},		/* opcode f7 */
	{"UNKN", &op_unknown},		/* opcode f8 */
	{"UNKN", &op_unknown},		/* opcode f9 */
	{"LD", &op_ld_imm},		/* opcode fa */
	{"EI", &op_ei},		/* opcode fb */
	{"UNKN", &op_unknown},		/* opcode fc */
	{"UNKN", &op_unknown},		/* opcode fd */
	{"CP", &op_cp_imm},		/* opcode fe */
	{"RST", &op_rst},		/* opcode ff */
};

static void dump_regs(void)
{
	int i;

	DPRINTF("; ");
	for (i=0; i<8; i++) {
		DPRINTF("%c=%02x ", regnames[i], regs.ri[i]);
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
		if (regs.ri[i] != oldregs.ri[i]) {
			if (i == 6) { /* Flags */
				if (regs.rn.f & ZF) DPRINTF("ZF ");
				else DPRINTF("zf ");
				if (regs.rn.f & SF) DPRINTF("SF ");
				else DPRINTF("sf ");
				if (regs.rn.f & HF) DPRINTF("HF ");
				else DPRINTF("hf ");
				if (regs.rn.f & CF) DPRINTF("CF ");
				else DPRINTF("cf ");
			} else {
				DPRINTF("%c=%02x ", regnames[i], regs.ri[i]);
			}
			oldregs.ri[i] = regs.ri[i];
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
	0x76,       /* halt */
	0x18, 0xfd, /* jr $-3 */
};

static void open_gbs(char *name, int i)
{
	int fd;
	unsigned char buf[0x70];
	struct stat st;
	unsigned int romsize;

	if ((fd = open(name, O_RDONLY)) == -1) {
		DPRINTF("Could not open %s: %s\n", name, strerror(errno));
		exit(1);
	}
	fstat(fd, &st);
	read(fd, buf, sizeof(buf));
	if (buf[0] != 'G' ||
	    buf[1] != 'B' ||
	    buf[2] != 'S' ||
	    buf[3] != 1) {
		DPRINTF("Not a GBS-File: %s\n", name);
		exit(1);
	}

	gbs_base  = buf[0x06] + (buf[0x07] << 8);
	gbs_init  = buf[0x08] + (buf[0x09] << 8);
	gbs_play  = buf[0x0a] + (buf[0x0b] << 8);
	gbs_stack = buf[0x0c] + (buf[0x0d] << 8);

	romsize = (st.st_size + gbs_base + 0x3fff) & ~0x3fff;

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
	       buf[0x05], buf[0x04],
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
		timertc = (256-buf[0x0e]) * (16 << (((buf[0x0f]+3) & 3) << 1));
		printf("Callback rate %2.2fHz (Custom).\n", 4194304/(float)timertc);
	} else {
		timertc = 70256;
		printf("Callback rate %2.2fHz (VBlank).\n", 4194304/(float)timertc);
	}

	rom = malloc(romsize);
	read(fd, &rom[gbs_base], st.st_size - 0x70);

	memcpy(rom, playercode, sizeof(playercode));

	REGS16_W(regs, PC, gbs_init);
	REGS16_W(regs, SP, gbs_stack);
	regs.rn.a  = (i - 1);
	push(0x0000); /* call return address */

	close(fd);
}

static char *notes[] = {
 "C-0","C#0","D-0","D#0","E-0","F-0","F#0","G-0","G#0","A-0","A#0","H-0",
 "C-1","C#1","D-1","D#1","E-1","F-1","F#1","G-1","G#1","A-1","A#1","H-1",
 "C-2","C#2","D-2","D#2","E-2","F-2","F#2","G-2","G#2","A-2","A#2","H-2",
 "C-3","C#3","D-3","D#3","E-3","F-3","F#3","G-3","G#3","A-3","A#3","H-3",
 "C-4","C#4","D-4","D#4","E-4","F-4","F#4","G-4","G#4","A-4","A#4","H-4",
 "C-5","C#5","D-5","D#5","E-5","F-5","F#5","G-5","G#5","A-5","A#5","H-5"
};

#define FREQ(x) (262144 / x)
#define NOTE(x) ((log(FREQ(x))/log(2)-log(130.67187)/log(2))*12 + .2)

char *vols[] = {
	"%%%#",
	"%%%=",
	"%%%-",
	"%%% ",
	"%%# ",
	"%%= ",
	"%%- ",
	"%%  ",
	"%#  ",
	"%=  ",
	"%-  ",
	"%   ",
	"#   ",
	"=   ",
	"-   ",
	"    ",
};

static int statustc = 83886;
static int statuscnt;

static unsigned char dmgwave[16] = {
	0xac, 0xdd, 0xda, 0x48,
	0x36, 0x02, 0xcf, 0x16,
	0x2c, 0x04, 0xe5, 0x2c,
	0xac, 0xdd, 0xda, 0x48
};

int main(int argc, char **argv)
{
	dspfd = open("/dev/dsp", O_WRONLY);
	int c;

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
	ch1.master = ch2.master = ch4.master = 1;

	memcpy(&ioregs[0x30], dmgwave, sizeof(dmgwave));

	if (argc != 3) {
		printf("Usage: %s <gbs-file> <subsong>\n", argv[0]);
		exit(1);
	}
	open_gbs(argv[1], argv[2][0]-'0');
	dump_regs();
	oldregs = regs;
	while (1) {
		if (!halted) {
			decode_ins();
			DEB(show_reg_diffs());
		} else cycles = 16;
		clock += cycles;
		timer -= cycles;
		statuscnt -= cycles;
		if (timer < 0) {
			timer += timertc;
			halted = 0;
			push(REGS16_R(regs, PC));
			REGS16_W(regs, PC, gbs_play);
		}
		if (statuscnt < 0) {
			int ni1 = (int)NOTE(ch1.div_tc);
			int ni2 = (int)NOTE(ch2.div_tc);
			int ni3 = (int)NOTE(ch3.div_tc);
			char *n1 = "---";
			char *n2 = "---";
			char *n3 = "---";

			statuscnt += statustc;

			if (ni1 >= 0 && ni1 < 12*6) n1 = notes[ni1];
			if (ni2 >= 0 && ni2 < 12*6) n2 = notes[ni2];
			if (ni3 >= 0 && ni3 < 12*6) n3 = notes[ni3];
			if (!ch1.volume) n1 = "---";
			if (!ch2.volume) n2 = "---";
			if (!ch3.volume) n3 = "---";

			printf("ch1:%s %s ch2:%s %s  ch3:%s %s ch4:%s\r",
				n1,
				vols[15-ch1.volume],
				n2,
				vols[15-ch2.volume],
				n3,
				vols[((ch3.volume+3)&3) << 2],
				vols[15-ch4.volume]);
			fflush(stdout);
		}
		do_sound(cycles);
		cycles = 0;
	}
	return 0;
}
