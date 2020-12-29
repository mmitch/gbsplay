/*
 * gbsplay is a Gameboy sound player
 *
 * 2003-2020 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                  Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <math.h>

#include "gbcpu.h"
#include "gbhw.h"
#include "impulse.h"

#define REG_TIMA 0x05
#define REG_TMA  0x06
#define REG_TAC  0x07
#define REG_IF   0x0f
#define REG_IE   0x7f /* Nominally 0xff, but we remap it to 0x7f internally. */

static const uint8_t ioregs_ormask[GBHW_IOREGS_SIZE] = {
	/* 0x00 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x10 */ 0x80, 0x3f, 0x00, 0xff, 0xbf,
	/* 0x15 */ 0xff, 0x3f, 0x00, 0xff, 0xbf,
	/* 0x1a */ 0x7f, 0xff, 0x9f, 0xff, 0xbf,
	/* 0x1f */ 0xff, 0xff, 0x00, 0x00, 0xbf,
	/* 0x24 */ 0x00, 0x00, 0x70, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};
static const uint8_t ioregs_initdata[GBHW_IOREGS_SIZE] = {
	/* 0x00 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* sound registers */
	/* 0x10 */ 0x80, 0xbf, 0x00, 0x00, 0xbf,
	/* 0x15 */ 0x00, 0x3f, 0x00, 0x00, 0xbf,
	/* 0x1a */ 0x7f, 0xff, 0x9f, 0x00, 0xbf,
	/* 0x1f */ 0x00, 0xff, 0x00, 0x00, 0xbf,
	/* 0x24 */ 0x77, 0xf3, 0xf1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* wave pattern memory, taken from gbsound.txt v0.99.19 (12/31/2002) */
	/* 0x30 */ 0xac, 0xdd, 0xda, 0x48, 0x36, 0x02, 0xcf, 0x16, 0x2c, 0x04, 0xe5, 0x2c, 0xac, 0xdd, 0xda, 0x48,
};

static const char dutylookup[4] = {
	1, 2, 4, 6
};
static const long len_mask[4] = {
	0x3f, 0x3f, 0xff, 0x3f
};

#define MASTER_VOL_MIN	0
#define MASTER_VOL_MAX	(256*256)

static const long vblanktc = 70224; /* ~59.73 Hz (vblankctr)*/
static const long vblankclocks = 4560;

static const long msec_cycles = GBHW_CLOCK/1000;

#define TAP1_15		0x4000;
#define TAP2_15		0x2000;
#define TAP1_7		0x0040;
#define TAP2_7		0x0020;

#define SOUND_DIV_MULT 0x10000LL

#define IMPULSE_WIDTH (1 << IMPULSE_W_SHIFT)
#define IMPULSE_N (1 << IMPULSE_N_SHIFT)
#define IMPULSE_N_MASK (IMPULSE_N - 1)

static const long main_div_tc = 32;
static const long sweep_div_tc = 256;

void gbhw_init_struct(struct gbhw *gbhw, struct gbs *gbs) {
	gbhw->gbs = gbs;

	gbhw->rombank = 1;
	gbhw->apu_on = 1;
	gbhw->io_written = 0;

	gbhw->filter_constant = GBHW_FILTER_CONST_DMG;
	gbhw->filter_enabled = 1;
	gbhw->cap_factor = 0x10000;

	gbhw->update_level = 0;
	gbhw->sequence_ctr = 0;
	gbhw->halted_noirq_cycles = 0;

	gbhw->timertc = 16;

	gbhw->rom_lockout = 1;

	gbhw->soundbuf = NULL; /* externally visible output buffer */
	gbhw->impbuf = NULL;   /* internal impulse output buffer */

	gbhw->tap1 = TAP1_15;
	gbhw->tap2 = TAP2_15;
	gbhw->lfsr = 0xffffffff;

	gbhw->sound_div_tc = 0;

	gbhw->last_l_value = 0;
	gbhw->last_r_value = 0;
	gbhw->ch3_next_nibble = 0;

	gbcpu_init_struct(&gbhw->gbcpu, gbhw);
}

static uint32_t rom_get(struct gbhw *gbhw, uint32_t addr)
{
	if ((addr >> 8) == 0 && gbhw->rom_lockout == 0) {
		return gbhw->boot_rom[addr & 0xff];
	}
//	DPRINTF("rom_get(%04x)\n", addr);
	return gbhw->rom[addr & 0x3fff];
}

static uint32_t rombank_get(struct gbhw *gbhw, uint32_t addr)
{
//	DPRINTF("rombank_get(%04x)\n", addr);
	return gbhw->rom[(addr & 0x3fff) + 0x4000*gbhw->rombank];
}

static uint32_t io_get(struct gbhw *gbhw, uint32_t addr)
{
	if (addr >= 0xff80 && addr <= 0xfffe) {
		return gbhw->hiram[addr & 0x7f];
	}
	if (addr >= 0xff10 &&
	           addr <= 0xff3f) {
		uint8_t val = gbhw->ioregs[addr & 0x7f];
		if (addr == 0xff26) {
			long i;
			val &= 0xf0;
			for (i=0; i<4; i++) {
				if (gbhw->ch[i].running) {
					val |= (1 << i);
				}
			}
		}
		val |= ioregs_ormask[addr & 0x7f];
		DPRINTF("io_get(%04x): %02x\n", addr, val);
		return val;
	}
	switch (addr) {
	case 0xff00:  // P1
		return 0;
	case 0xff05:  // TIMA
	case 0xff06:  // TMA
	case 0xff07:  // TAC
	case 0xff0f:  // IF
		return gbhw->ioregs[addr & 0x7f];
	case 0xff41: /* LCDC Status */
		if (gbhw->vblankctr > vblanktc - vblankclocks) {
			return 0x01;  /* vblank */
		} else {
			/* ~108.7uS per line */
			long t = (2 * vblanktc - gbhw->vblankctr) % 456;
			if (t < 204) {
				/* 48.6uS in hblank (201-207 clks) */
				return 0x00;
			} else if (t < 284) {
				/* 19uS in OAM scan (77-83 clks) */
				return 0x02;
			}
		}
		return 0x03;  /* both OAM and display RAM busy */
	case 0xff44: /* LCD Y-coordinate */
		return ((2 * vblanktc - vblankclocks - gbhw->vblankctr) / 456) % 154;
	case 0xff70:  // CGB ram bank switch
		WARN_ONCE("ioread from SVBK (CGB mode) ignored.\n");
		return 0xff;
	case 0xffff:
		return gbhw->ioregs[0x7f];
	default:
		WARN_ONCE("ioread from 0x%04x unimplemented.\n", (unsigned int)addr);
		DPRINTF("io_get(%04x)\n", addr);
		return 0xff;
	}
}

static uint32_t intram_get(struct gbhw *gbhw, uint32_t addr)
{
//	DPRINTF("intram_get(%04x)\n", addr);
	return gbhw->intram[addr & 0x1fff];
}

static uint32_t extram_get(struct gbhw *gbhw, uint32_t addr)
{
//	DPRINTF("extram_get(%04x)\n", addr);
	return gbhw->extram[addr & 0x1fff];
}

static void rom_put(struct gbhw *gbhw, uint32_t addr, uint8_t val)
{
	if (addr >= 0x2000 && addr <= 0x3fff) {
		val &= 0x1f;
		gbhw->rombank = val + (val == 0);
		if (gbhw->rombank > gbhw->lastbank) {
			WARN_ONCE("Bank %ld out of range (0-%ld)!\n", gbhw->rombank, gbhw->lastbank);
			gbhw->rombank = gbhw->lastbank;
		}
	} else {
		WARN_ONCE("rom write of %02x to %04x ignored\n", val, addr);
	}
}

static void apu_reset(struct gbhw *gbhw)
{
	long i;
	int mute_tmp[4];

	for (i = 0; i < 4; i++) {
		mute_tmp[i] = gbhw->ch[i].mute;
	}
	memset(gbhw->ch, 0, sizeof(struct gbhw_channel) * 4);
	for (i = 0xff10; i < 0xff26; i++) {
		gbhw->ioregs[i & 0x7f] = 0;
	}
	for (i = 0; i < 4; i++) {
		gbhw->ch[i].len = 0;
		gbhw->ch[i].len_gate = 0;
		gbhw->ch[i].volume = 0;
		gbhw->ch[i].env_volume = 0;
		gbhw->ch[i].duty_ctr = 4;
		gbhw->ch[i].div_tc = 1;
		gbhw->ch[i].master = 1;
		gbhw->ch[i].running = 0;
		gbhw->ch[i].mute = mute_tmp[i];
	}
	gbhw->sequence_ctr = 0;
}

static void linkport_atexit(void);

static void linkport_write(long c)
{
	static char buf[256];
	static unsigned long idx = 0;
	static long exit_handler_set = 0;
	static long enabled = 1;

	if (!enabled) {
		return;
	}
	if (!(c == -1 || c == '\r' || c == '\n' || (c >= 0x20 && c <= 0x7f))) {
		enabled = 0;
		fprintf(stderr, "Link port output %02lx ignored.\n", c);
		return;
	}
	if (c != -1 && idx < (sizeof(buf) - 1)) {
		buf[idx++] = c;
		buf[idx] = 0;
	}
	if (c == '\n' || (c == -1 && idx > 0)) {
		fprintf(stderr, "Link port text: %s", buf);
		idx = 0;
		if (!exit_handler_set) {
			atexit(linkport_atexit);
		}
	}
}

static void linkport_atexit(void)
{
	linkport_write(-1);
}

static void sequencer_update_len(struct gbhw *gbhw, long chn)
{
	if (gbhw->ch[chn].len_enable && gbhw->ch[chn].len_gate) {
		gbhw->ch[chn].len++;
		gbhw->ch[chn].len &= len_mask[chn];
		if (gbhw->ch[chn].len == 0) {
			gbhw->ch[chn].env_volume = 0;
			gbhw->ch[chn].env_tc = 0;
			gbhw->ch[chn].running = 0;
			gbhw->ch[chn].len_gate = 0;
		}
	}
}

static long sweep_check_overflow(struct gbhw *gbhw)
{
	long val = (2048 - gbhw->ch[0].div_tc_shadow) >> gbhw->ch[0].sweep_shift;

	if (gbhw->ch[0].sweep_shift == 0) {
		return 1;
	}

	if (!gbhw->ch[0].sweep_dir) {
		if (gbhw->ch[0].div_tc_shadow <= val) {
			gbhw->ch[0].running = 0;
			return 0;
		}
	}
	return 1;
}

static void io_put(struct gbhw *gbhw, uint32_t addr, uint8_t val)
{
	long chn = (addr - 0xff10)/5;

	if (addr >= 0xff80 && addr <= 0xfffe) {
		gbhw->hiram[addr & 0x7f] = val;
		return;
	}

	gbhw->io_written = 1;

	if (gbhw->iocallback)
		gbhw->iocallback(gbhw->sum_cycles, addr, val, gbhw->iocallback_priv);

	if (gbhw->apu_on == 0 && addr >= 0xff10 && addr < 0xff26) {
		return;
	}
	gbhw->ioregs[addr & 0x7f] = val;
	DPRINTF(" ([0x%04x]=%02x) ", addr, val);
	switch (addr) {
		case 0xff02:
			if (val & 0x80) {
				linkport_write(gbhw->ioregs[1]);
			}
			break;
		case 0xff05:  // TIMA
		case 0xff06:  // TMA
			break;
		case 0xff07:  // TAC
			gbhw->timertc = 16 << (((val+3) & 3) << 1);
			if ((val & 0xf0) == 0x80) gbhw->timertc /= 2;
			if (gbhw->timerctr > gbhw->timertc) {
				gbhw->timerctr = 0;
			}
			break;
		case 0xff0f:  // IF
			break;
		case 0xff10:
			gbhw->ch[0].sweep_ctr = gbhw->ch[0].sweep_tc = ((val >> 4) & 7);
			gbhw->ch[0].sweep_dir = (val >> 3) & 1;
			gbhw->ch[0].sweep_shift = val & 7;

			break;
		case 0xff11:
		case 0xff16:
		case 0xff20:
			{
				long duty_ctr = val >> 6;
				long len = val & 0x3f;

				gbhw->ch[chn].duty_ctr = dutylookup[duty_ctr];
				gbhw->ch[chn].duty_tc = gbhw->ch[chn].div_tc*gbhw->ch[chn].duty_ctr/8;
				gbhw->ch[chn].len = len;
				gbhw->ch[chn].len_gate = 1;

				break;
			}
		case 0xff12:
		case 0xff17:
		case 0xff21:
			{
				long vol = val >> 4;
				long envdir = (val >> 3) & 1;
				long envspd = val & 7;

				gbhw->ch[chn].volume = vol;
				gbhw->ch[chn].env_dir = envdir;
				gbhw->ch[chn].env_ctr = gbhw->ch[chn].env_tc = envspd;

				gbhw->ch[chn].master = (val & 0xf8) != 0;
				if (!gbhw->ch[chn].master) {
					gbhw->ch[chn].running = 0;
				}
			}
			break;
		case 0xff13:
		case 0xff14:
		case 0xff18:
		case 0xff19:
		case 0xff1d:
		case 0xff1e:
			{
				long div = gbhw->ioregs[0x13 + 5*chn];
				long old_len_enable = gbhw->ch[chn].len_enable;

				div |= ((long)gbhw->ioregs[0x14 + 5*chn] & 7) << 8;
				gbhw->ch[chn].div_tc = 2048 - div;
				gbhw->ch[chn].duty_tc = gbhw->ch[chn].div_tc*gbhw->ch[chn].duty_ctr/8;

				if (addr == 0xff13 ||
				    addr == 0xff18 ||
				    addr == 0xff1d) break;

				gbhw->ch[chn].len_enable = (gbhw->ioregs[0x14 + 5*chn] & 0x40) > 0;
				if ((val & 0x80) == 0x80) {
					gbhw->ch[chn].env_volume = gbhw->ch[chn].volume;
					if (!gbhw->ch[chn].len_gate) {
						gbhw->ch[chn].len_gate = 1;
						if (old_len_enable == 1 &&
						    gbhw->ch[chn].len_enable == 1 &&
						    (gbhw->sequence_ctr & 1) == 1) {
							// Trigger that un-freezes enabled length should clock it
							sequencer_update_len(gbhw, chn);
						}
					}
					if (gbhw->ch[chn].master) {
						gbhw->ch[chn].running = 1;
					}
					if (addr == 0xff1e) {
						gbhw->ch3pos = 0;
					}
					if (addr == 0xff14) {
						gbhw->ch[0].div_tc_shadow = gbhw->ch[0].div_tc;
						sweep_check_overflow(gbhw);
					}
				}
				if (old_len_enable == 0 &&
				    gbhw->ch[chn].len_enable == 1 &&
				    (gbhw->sequence_ctr & 1) == 1) {
					// Enabling in first half of length period should clock length
					sequencer_update_len(gbhw, chn);
				}
			}

//			printf(" ch%ld: vol=%02d envd=%ld envspd=%ld duty_ctr=%ld len=%03d len_en=%ld key=%04d gate=%ld%ld\n", chn, gbhw->ch[chn].volume, gbhw->ch[chn].env_dir, gbhw->ch[chn].env_tc, gbhw->ch[chn].duty_ctr, gbhw->ch[chn].len, gbhw->ch[chn].len_enable, gbhw->ch[chn].div_tc, gbhw->ch[chn].leftgate, gbhw->ch[chn].rightgate);
			break;
		case 0xff15:
			break;
		case 0xff1a:
			gbhw->ch[2].master = (gbhw->ioregs[0x1a] & 0x80) > 0;
			if (!gbhw->ch[2].master) {
				gbhw->ch[2].running = 0;
			}
			break;
		case 0xff1b:
			gbhw->ch[2].len = val;
			gbhw->ch[2].len_gate = 1;
			break;
		case 0xff1c:
			{
				long vol = (gbhw->ioregs[0x1c] >> 5) & 3;
				gbhw->ch[2].env_volume = gbhw->ch[2].volume = vol;
				break;
			}
		case 0xff1f:
			break;
		case 0xff22:
		case 0xff23:
			{
				long reg = gbhw->ioregs[0x22];
				long shift = reg >> 4;
				long rate = reg & 7;
				long old_len_enable = gbhw->ch[chn].len_enable;
				gbhw->ch[3].div_ctr = 0;
				gbhw->ch[3].div_tc = 16 << shift;
				if (reg & 8) {
					gbhw->tap1 = TAP1_7;
					gbhw->tap2 = TAP2_7;
				} else {
					gbhw->tap1 = TAP1_15;
					gbhw->tap2 = TAP2_15;
				}
				if (rate) gbhw->ch[3].div_tc *= rate;
				else gbhw->ch[3].div_tc /= 2;
				gbhw->ch[chn].len_enable = (gbhw->ioregs[0x23] & 0x40) > 0;
				if (addr == 0xff22) break;

				if (val & 0x80) {  /* trigger */
					gbhw->lfsr = 0xffffffff;
					gbhw->ch[chn].env_volume = gbhw->ch[chn].volume;
					if (!gbhw->ch[chn].len_gate) {
						gbhw->ch[chn].len_gate = 1;
						if (old_len_enable == 1 &&
						    gbhw->ch[chn].len_enable == 1 &&
						    (gbhw->sequence_ctr & 1) == 1) {
							// Trigger that un-freezes enabled length should clock it
							sequencer_update_len(gbhw, chn);
						}
					}
					if (gbhw->ch[3].master) {
						gbhw->ch[3].running = 1;
					}
				}
				if (old_len_enable == 0 &&
				    gbhw->ch[chn].len_enable == 1 &&
				    (gbhw->sequence_ctr & 1) == 1) {
					// Enabling in first half of length period should clock length
					sequencer_update_len(gbhw, chn);
				}
//				printf(" ch4: vol=%02d envd=%ld envspd=%ld duty_ctr=%ld len=%03d len_en=%ld key=%04d gate=%ld%ld\n", gbhw->ch[3].volume, gbhw->ch[3].env_dir, gbhw->ch[3].env_ctr, gbhw->ch[3].duty_ctr, gbhw->ch[3].len, gbhw->ch[3].len_enable, gbhw->ch[3].div_tc, gbhw->ch[3].leftgate, gbhw->ch[3].rightgate);
			}
			break;
		case 0xff25:
			gbhw->ch[0].leftgate = (val & 0x10) > 0;
			gbhw->ch[0].rightgate = (val & 0x01) > 0;
			gbhw->ch[1].leftgate = (val & 0x20) > 0;
			gbhw->ch[1].rightgate = (val & 0x02) > 0;
			gbhw->ch[2].leftgate = (val & 0x40) > 0;
			gbhw->ch[2].rightgate = (val & 0x04) > 0;
			gbhw->ch[3].leftgate = (val & 0x80) > 0;
			gbhw->ch[3].rightgate = (val & 0x08) > 0;
			gbhw->update_level = 1;
			break;
		case 0xff26:
			if (val & 0x80) {
				gbhw->ioregs[0x26] = 0x80;
				gbhw->apu_on = 1;
			} else {
				apu_reset(gbhw);
				gbhw->apu_on = 0;
			}
			break;
		case 0xff70:
			WARN_ONCE("iowrite to SVBK (CGB mode) ignored.\n");
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
		case 0xff50: /* bootrom lockout reg */
			if (val == 0x01) {
				gbhw->rom_lockout = 1;
			}
			break;
		case 0xffff:
			break;
		default:
			WARN_ONCE("iowrite to 0x%04x unimplemented (val=%02x).\n", addr, val);
			break;
	}
}

static void intram_put(struct gbhw *gbhw, uint32_t addr, uint8_t val)
{
	gbhw->intram[addr & 0x1fff] = val;
}

static void extram_put(struct gbhw *gbhw, uint32_t addr, uint8_t val)
{
	gbhw->extram[addr & 0x1fff] = val;
}

static void sequencer_step(struct gbhw *gbhw)
{
	long i;
	long clock_len = 0;
	long clock_env = 0;
	long clock_sweep = 0;

	switch (gbhw->sequence_ctr & 7) {
	case 0: clock_len = 1; break;
	case 1: break;
	case 2: clock_len = 1; clock_sweep = 1; break;
	case 3: break;
	case 4: clock_len = 1; break;
	case 5: break;
	case 6: clock_len = 1; clock_sweep = 1; break;
	case 7: clock_env = 1; break;
	}

	gbhw->sequence_ctr++;

	if (clock_sweep && gbhw->ch[0].sweep_tc) {
		gbhw->ch[0].sweep_ctr--;
		if (gbhw->ch[0].sweep_ctr < 0) {
			long val = (2048 - gbhw->ch[0].div_tc_shadow) >> gbhw->ch[0].sweep_shift;

			gbhw->ch[0].sweep_ctr = gbhw->ch[0].sweep_tc;
			if (sweep_check_overflow(gbhw)) {
				if (gbhw->ch[0].sweep_dir) {
					gbhw->ch[0].div_tc_shadow += val;
				} else {
					gbhw->ch[0].div_tc_shadow -= val;
				}
				gbhw->ch[0].div_tc = gbhw->ch[0].div_tc_shadow;
			}
			gbhw->ch[0].duty_tc = gbhw->ch[0].div_tc*gbhw->ch[0].duty_ctr/8;
		}
	}
	for (i=0; clock_len && i<4; i++) {
		sequencer_update_len(gbhw, i);
	}
	for (i=0; clock_env && i<4; i++) {
		if (gbhw->ch[i].env_tc) {
			gbhw->ch[i].env_ctr--;
			if (gbhw->ch[i].env_ctr <=0 ) {
				gbhw->ch[i].env_ctr = gbhw->ch[i].env_tc;
				if (gbhw->ch[i].running) {
					if (!gbhw->ch[i].env_dir) {
						if (gbhw->ch[i].env_volume > 0)
							gbhw->ch[i].env_volume--;
					} else {
						if (gbhw->ch[i].env_volume < 15)
						gbhw->ch[i].env_volume++;
					}
				}
			}
		}
	}
	if (gbhw->master_fade) {
		gbhw->master_volume += gbhw->master_fade;
		if ((gbhw->master_fade > 0 &&
		     gbhw->master_volume >= gbhw->master_dstvol) ||
		    (gbhw->master_fade < 0 &&
		     gbhw->master_volume <= gbhw->master_dstvol)) {
			gbhw->master_fade = 0;
			gbhw->master_volume = gbhw->master_dstvol;
		}
	}
}

void gbhw_master_fade(struct gbhw *gbhw, long speed, long dstvol)
{
	if (dstvol < MASTER_VOL_MIN) dstvol = MASTER_VOL_MIN;
	if (dstvol > MASTER_VOL_MAX) dstvol = MASTER_VOL_MAX;
	gbhw->master_dstvol = dstvol;
	if (dstvol > gbhw->master_volume)
		gbhw->master_fade = speed;
	else gbhw->master_fade = -speed;
}

#define GET_NIBBLE(p, n) ({ \
	long index = ((n) >> 1) & 0xf; \
	long shift = (~(n) & 1) << 2; \
	(((p)[index] >> shift) & 0xf); })

static void gb_flush_buffer(struct gbhw *gbhw)
{
	long i;
	long overlap;
	long l_smpl, r_smpl;
	long l_cap, r_cap;

	assert(gbhw->soundbuf != NULL);
	assert(gbhw->impbuf != NULL);

	/* integrate buffer */
	l_smpl = gbhw->soundbuf->l_lvl;
	r_smpl = gbhw->soundbuf->r_lvl;
	l_cap = gbhw->soundbuf->l_cap;
	r_cap = gbhw->soundbuf->r_cap;
	for (i=0; i<gbhw->soundbuf->samples; i++) {
		long l_out, r_out;
		l_smpl = l_smpl + gbhw->impbuf->data[i*2  ];
		r_smpl = r_smpl + gbhw->impbuf->data[i*2+1];
		if (gbhw->filter_enabled && gbhw->cap_factor <= 0x10000) {
			/*
			 * RC High-pass & DC decoupling filter. Gameboy
			 * Classic uses 1uF and 510 Ohms in series,
			 * followed by 10K Ohms pot to ground between
			 * CPU output and amplifier input, which gives a
			 * cutoff frequency of 15.14Hz.
			 */
			l_out = l_smpl - (l_cap >> 16);
			r_out = r_smpl - (r_cap >> 16);
			/* cap factor is 0x10000 for a factor of 1.0 */
			l_cap = (l_smpl << 16) - l_out * gbhw->cap_factor;
			r_cap = (r_smpl << 16) - r_out * gbhw->cap_factor;
		} else {
			l_out = l_smpl;
			r_out = r_smpl;
		}
		gbhw->soundbuf->data[i*2  ] = l_out * gbhw->master_volume / MASTER_VOL_MAX;
		gbhw->soundbuf->data[i*2+1] = r_out * gbhw->master_volume / MASTER_VOL_MAX;
		if (l_out > gbhw->lmaxval) gbhw->lmaxval = l_out;
		if (l_out < gbhw->lminval) gbhw->lminval = l_out;
		if (r_out > gbhw->rmaxval) gbhw->rmaxval = r_out;
		if (r_out < gbhw->rminval) gbhw->rminval = r_out;
	}
	gbhw->soundbuf->pos = gbhw->soundbuf->samples;
	gbhw->soundbuf->l_lvl = l_smpl;
	gbhw->soundbuf->r_lvl = r_smpl;
	gbhw->soundbuf->l_cap = l_cap;
	gbhw->soundbuf->r_cap = r_cap;

	if (gbhw->callback != NULL) gbhw->callback(gbhw->gbs, gbhw->callbackpriv);

	overlap = gbhw->impbuf->samples - gbhw->soundbuf->samples;
	memmove(gbhw->impbuf->data, gbhw->impbuf->data+(2*gbhw->soundbuf->samples), 4*overlap);
	memset(gbhw->impbuf->data + 2*overlap, 0, gbhw->impbuf->bytes - 4*overlap);
	assert(gbhw->impbuf->bytes == gbhw->impbuf->samples*4);
	assert(gbhw->soundbuf->bytes == gbhw->soundbuf->samples*4);
	memset(gbhw->soundbuf->data, 0, gbhw->soundbuf->bytes);
	gbhw->soundbuf->pos = 0;

	gbhw->impbuf->cycles -= (gbhw->sound_div_tc * gbhw->soundbuf->samples) / SOUND_DIV_MULT;
}

static void gb_change_level(struct gbhw *gbhw, long l_ofs, long r_ofs)
{
	long pos;
	long imp_idx;
	long imp_l = -IMPULSE_WIDTH/2;
	long imp_r = IMPULSE_WIDTH/2;
	long i;
	const short *ptr = base_impulse;

	assert(gbhw->impbuf != NULL);
	pos = (long)(gbhw->impbuf->cycles * SOUND_DIV_MULT / gbhw->sound_div_tc);
	imp_idx = (long)((gbhw->impbuf->cycles << IMPULSE_N_SHIFT)*SOUND_DIV_MULT / gbhw->sound_div_tc) & IMPULSE_N_MASK;
	assert(pos + imp_r < gbhw->impbuf->samples);
	assert(pos + imp_l >= 0);

	ptr += imp_idx * IMPULSE_WIDTH;

	for (i=imp_l; i<imp_r; i++) {
		long bufi = pos + i;
		long impi = i + IMPULSE_WIDTH/2;
		gbhw->impbuf->data[bufi*2  ] += ptr[impi] * l_ofs;
		gbhw->impbuf->data[bufi*2+1] += ptr[impi] * r_ofs;
	}

	gbhw->impbuf->l_lvl += l_ofs*256;
	gbhw->impbuf->r_lvl += r_ofs*256;
}

static void gb_sound(struct gbhw *gbhw, long cycles)
{
	long i, j;
	long l_lvl = 0, r_lvl = 0;

	assert(gbhw->impbuf != NULL);

	for (j=0; j<cycles; j++) {
		gbhw->main_div++;
		gbhw->impbuf->cycles++;
		if (gbhw->impbuf->cycles*SOUND_DIV_MULT >= gbhw->sound_div_tc*(gbhw->impbuf->samples - IMPULSE_WIDTH/2))
			gb_flush_buffer(gbhw);

		if (gbhw->ch[2].running) {
			gbhw->ch[2].div_ctr--;
			if (gbhw->ch[2].div_ctr <= 0) {
				long val = gbhw->ch3_next_nibble;
				long pos = gbhw->ch3pos++;
				gbhw->ch3_next_nibble = GET_NIBBLE(&gbhw->ioregs[0x30], pos) * 2;
				gbhw->ch[2].div_ctr = gbhw->ch[2].div_tc*2;
				if (gbhw->ch[2].env_volume) {
					val = val >> (gbhw->ch[2].env_volume-1);
				} else val = 0;
				gbhw->ch[2].lvl = val - 15;
				gbhw->update_level = 1;
			}
		}

		if (gbhw->ch[3].running) {
			gbhw->ch[3].div_ctr--;
			if (gbhw->ch[3].div_ctr <= 0) {
				long val;
				gbhw->ch[3].div_ctr = gbhw->ch[3].div_tc;
				gbhw->lfsr = (gbhw->lfsr << 1) | (((gbhw->lfsr & gbhw->tap1) > 0) ^ ((gbhw->lfsr & gbhw->tap2) > 0));
				val = gbhw->ch[3].env_volume * 2 * (!(gbhw->lfsr & gbhw->tap1));
				gbhw->ch[3].lvl = val - 15;
				gbhw->update_level = 1;
			}
		}

		if (gbhw->main_div > main_div_tc) {
			gbhw->main_div -= main_div_tc;

			for (i=0; i<2; i++) if (gbhw->ch[i].running) {
				long val = 2 * gbhw->ch[i].env_volume;
				if (gbhw->ch[i].div_ctr > gbhw->ch[i].duty_tc) {
					val = 0;
				}
				gbhw->ch[i].lvl = val - 15;
				gbhw->ch[i].div_ctr--;
				if (gbhw->ch[i].div_ctr <= 0) {
					gbhw->ch[i].div_ctr = gbhw->ch[i].div_tc;
				}
			}

			gbhw->sweep_div += 1;
			if (gbhw->sweep_div >= sweep_div_tc) {
				gbhw->sweep_div = 0;
				sequencer_step(gbhw);
			}
			gbhw->update_level = 1;
		}

		if (gbhw->update_level) {
			gbhw->update_level = 0;
			l_lvl = 0;
			r_lvl = 0;
			for (i=0; i<4; i++) {
				if (gbhw->ch[i].mute)
					continue;
				if (gbhw->ch[i].leftgate)
					l_lvl += gbhw->ch[i].lvl;
				if (gbhw->ch[i].rightgate)
					r_lvl += gbhw->ch[i].lvl;
			}

			if (l_lvl != gbhw->last_l_value || r_lvl != gbhw->last_r_value) {
				gb_change_level(gbhw, l_lvl - gbhw->last_l_value, r_lvl - gbhw->last_r_value);
				gbhw->last_l_value = l_lvl;
				gbhw->last_r_value = r_lvl;
			}
		}
	}
}

void gbhw_set_callback(struct gbhw *gbhw, gbhw_callback_fn fn, void *priv)
{
	gbhw->callback = fn;
	gbhw->callbackpriv = priv;
}

void gbhw_set_io_callback(struct gbhw *gbhw, gbhw_iocallback_fn fn, void *priv)
{
	gbhw->iocallback = fn;
	gbhw->iocallback_priv = priv;
}

void gbhw_set_step_callback(struct gbhw *gbhw, gbhw_stepcallback_fn fn, void *priv)
{
	gbhw->stepcallback = fn;
	gbhw->stepcallback_priv = priv;
}

static void gbhw_impbuf_reset(struct gbhw *gbhw)
{
	assert(gbhw->sound_div_tc != 0);
	gbhw->impbuf->cycles = (long)(gbhw->sound_div_tc * IMPULSE_WIDTH/2 / SOUND_DIV_MULT);
	gbhw->impbuf->l_lvl = 0;
	gbhw->impbuf->r_lvl = 0;
	memset(gbhw->impbuf->data, 0, gbhw->impbuf->bytes);
}

void gbhw_set_buffer(struct gbhw *gbhw, struct gbhw_buffer *buffer)
{
	gbhw->soundbuf = buffer;
	gbhw->soundbuf->samples = gbhw->soundbuf->bytes / 4;

	if (gbhw->impbuf) free(gbhw->impbuf);
	gbhw->impbuf = malloc(sizeof(*gbhw->impbuf) + (gbhw->soundbuf->samples + IMPULSE_WIDTH + 1) * 4);
	if (gbhw->impbuf == NULL) {
		fprintf(stderr, "%s", _("Memory allocation failed!\n"));
		return;
	}
	memset(gbhw->impbuf, 0, sizeof(*gbhw->impbuf));
	gbhw->impbuf->data = (void*)(gbhw->impbuf+1);
	gbhw->impbuf->samples = gbhw->soundbuf->samples + IMPULSE_WIDTH + 1;
	gbhw->impbuf->bytes = gbhw->impbuf->samples * 4;
	gbhw_impbuf_reset(gbhw);
}

static void gbhw_update_filter(struct gbhw *gbhw)
{
	double cap_constant = pow(gbhw->filter_constant, (double)GBHW_CLOCK / gbhw->sample_rate);
	gbhw->cap_factor = round(65536.0 * cap_constant);
}

long gbhw_set_filter(struct gbhw *gbhw, const char *type)
{
	if (strcasecmp(type, GBHW_CFG_FILTER_OFF) == 0) {
		gbhw->filter_enabled = 0;
		gbhw->filter_constant = GBHW_FILTER_CONST_OFF;
	} else if (strcasecmp(type, GBHW_CFG_FILTER_DMG) == 0) {
		gbhw->filter_enabled = 1;
		gbhw->filter_constant = GBHW_FILTER_CONST_DMG;
	} else if (strcasecmp(type, GBHW_CFG_FILTER_CGB) == 0) {
		gbhw->filter_enabled = 1;
		gbhw->filter_constant = GBHW_FILTER_CONST_CGB;
	} else {
		return 0;
	}

	gbhw_update_filter(gbhw);

	return 1;
}

void gbhw_set_rate(struct gbhw *gbhw, long rate)
{
	gbhw->sample_rate = rate;
	gbhw->sound_div_tc = GBHW_CLOCK*SOUND_DIV_MULT/rate;
	gbhw_update_filter(gbhw);
}

void gbhw_getminmax(struct gbhw *gbhw, int16_t *lmin, int16_t *lmax, int16_t *rmin, int16_t *rmax)
{
	if (gbhw->lminval == INT_MAX) return;
	*lmin = gbhw->lminval;
	*lmax = gbhw->lmaxval;
	*rmin = gbhw->rminval;
	*rmax = gbhw->rmaxval;
	gbhw->lminval = gbhw->rminval = INT_MAX;
	gbhw->lmaxval = gbhw->rmaxval = INT_MIN;
}

/*
 * Initialize Gameboy hardware emulation.
 * The size should be a multiple of 0x4000,
 * so we don't need range checking in rom_get and
 * rombank_get.
 */
void gbhw_init(struct gbhw *gbhw, uint8_t *rombuf, uint32_t size)
{
	long i;

	gbhw->vblankctr = vblanktc;
	gbhw->timerctr = 0;

	if (gbhw->impbuf)
		gbhw_impbuf_reset(gbhw);
	gbhw->rom = rombuf;
	gbhw->lastbank = ((size + 0x3fff) / 0x4000) - 1;
	gbhw->rombank = 1;
	gbhw->master_volume = MASTER_VOL_MAX;
	gbhw->master_fade = 0;
	gbhw->apu_on = 1;
	if (gbhw->soundbuf) {
		gbhw->soundbuf->pos = 0;
		gbhw->soundbuf->l_lvl = 0;
		gbhw->soundbuf->r_lvl = 0;
		gbhw->soundbuf->l_cap = 0;
		gbhw->soundbuf->r_cap = 0;
	}
	gbhw->lminval = gbhw->rminval = INT_MAX;
	gbhw->lmaxval = gbhw->rmaxval = INT_MIN;
	apu_reset(gbhw);
	memset(gbhw->extram, 0, sizeof(uint8_t) * GBHW_EXTRAM_SIZE);
	memset(gbhw->intram, 0, sizeof(uint8_t) * GBHW_INTRAM_SIZE);
	memset(gbhw->hiram, 0, sizeof(uint8_t) * GBHW_HIRAM_SIZE);
	memset(gbhw->ioregs, 0, sizeof(uint8_t) * GBHW_IOREGS_SIZE);
	for (i=0x10; i<0x40; i++) {
		io_put(gbhw, 0xff00 + i, ioregs_initdata[i]);
	}

	gbhw->sum_cycles = 0;
	gbhw->halted_noirq_cycles = 0;
	gbhw->ch3pos = 0;
	gbhw->ch3_next_nibble = 0;
	gbhw->last_l_value = 0;
	gbhw->last_r_value = 0;

	gbcpu_init(&gbhw->gbcpu);
	gbcpu_add_mem(&gbhw->gbcpu, 0x00, 0x3f, rom_put, rom_get);
	gbcpu_add_mem(&gbhw->gbcpu, 0x40, 0x7f, rom_put, rombank_get);
	gbcpu_add_mem(&gbhw->gbcpu, 0xa0, 0xbf, extram_put, extram_get);
	gbcpu_add_mem(&gbhw->gbcpu, 0xc0, 0xfe, intram_put, intram_get);
	gbcpu_add_mem(&gbhw->gbcpu, 0xff, 0xff, io_put, io_get);
}

void gbhw_enable_bootrom(struct gbhw *gbhw, const uint8_t *rombuf)
{
	memcpy(gbhw->boot_rom, rombuf, sizeof(uint8_t) * GBHW_BOOT_ROM_SIZE);
	gbhw->rom_lockout = 0;
}

/* internal for gbs.c, not exported from libgbs */
void gbhw_io_put(struct gbhw *gbhw, uint16_t addr, uint8_t val) {
	if (addr != 0xffff && (addr < 0xff00 || addr > 0xff7f))
		return;
	io_put(gbhw, addr, val);
}

/* unmasked peek used by gbsplay.c to print regs */
uint8_t gbhw_io_peek(struct gbhw *gbhw, uint16_t addr)
{
	if (addr >= 0xff10 && addr <= 0xff3f) {
		return gbhw->ioregs[addr & 0x7f];
	}
	return 0xff;
}


void gbhw_check_if(struct gbhw *gbhw)
{
	struct gbcpu *gbcpu = &gbhw->gbcpu;

	uint8_t mask = 0x01; /* lowest bit is highest priority irq */
	uint8_t vec = 0x40;
	if (!gbcpu->ime) {
		/* interrupts disabled */
		if (gbhw->ioregs[REG_IF] & gbhw->ioregs[REG_IE]) {
			/* but will still exit halt */
			gbcpu->halted = 0;
		}
		return;
	}
	while (mask <= 0x10) {
		if (gbhw->ioregs[REG_IF] & gbhw->ioregs[REG_IE] & mask) {
			gbhw->ioregs[REG_IF] &= ~mask;
			gbcpu->halted = 0;
			gbcpu_intr(gbcpu, vec);
			break;
		}
		vec += 0x08;
		mask <<= 1;
	}
}

static void blargg_debug(struct gbcpu *gbcpu)
{
	long i;

	/* Blargg GB debug output signature. */
	if (gbcpu_mem_get(gbcpu, 0xa001) != 0xde ||
	    gbcpu_mem_get(gbcpu, 0xa002) != 0xb0 ||
	    gbcpu_mem_get(gbcpu, 0xa003) != 0x61) {
		return;
	}

	fprintf(stderr, "\nBlargg debug output:\n");

	for (i = 0xa004; i < 0xb000; i++) {
		uint8_t c = gbcpu_mem_get(gbcpu, i);
		if (c == 0 || c >= 128) {
			return;
		}
		if (c < 32 && c != 10 && c != 13) {
			return;
		}
		fputc(c, stderr);
	}
}

/**
 * @param time_to_work  emulated time in milliseconds
 * @return  elapsed cpu cycles
 */
long gbhw_step(struct gbhw *gbhw, long time_to_work)
{
	struct gbcpu *gbcpu = &gbhw->gbcpu;
	long cycles_total = 0;

	time_to_work *= msec_cycles;
	
	while (cycles_total < time_to_work) {
		long maxcycles = time_to_work - cycles_total;
		long cycles = 0;

		if (gbhw->vblankctr > 0 && gbhw->vblankctr < maxcycles) maxcycles = gbhw->vblankctr;
		if (gbhw->timerctr > 0 && gbhw->timerctr < maxcycles) maxcycles = gbhw->timerctr;

		gbhw->io_written = 0;
		while (cycles < maxcycles && !gbhw->io_written) {
			long step;
			gbhw_check_if(gbhw);
			step = gbcpu_step(gbcpu);
			if (gbcpu->halted) {
				gbhw->halted_noirq_cycles += step;
				if (gbcpu->ime == 0 &&
				    (gbhw->ioregs[REG_IE] == 0 ||
				     gbhw->halted_noirq_cycles > GBHW_CLOCK/10)) {
					fprintf(stderr, "CPU locked up (halt with interrupts disabled).\n");
					blargg_debug(gbcpu);
					return -1;
				}
			} else {
				gbhw->halted_noirq_cycles = 0;
			}
			if (step < 0) return step;
			cycles += step;
			gbhw->sum_cycles += step;
			gbhw->vblankctr -= step;
			if (gbhw->vblankctr <= 0) {
				gbhw->vblankctr += vblanktc;
				gbhw->ioregs[REG_IF] |= 0x01;
				DPRINTF("vblank_interrupt\n");
			}
			gb_sound(gbhw, step);
			if (gbhw->stepcallback)
			   gbhw->stepcallback(gbhw->sum_cycles, gbhw->ch, gbhw->stepcallback_priv);
		}

		if (gbhw->ioregs[REG_TAC] & 4) {
			if (gbhw->timerctr > 0) gbhw->timerctr -= cycles;
			while (gbhw->timerctr <= 0) {
				gbhw->timerctr += gbhw->timertc;
				gbhw->ioregs[REG_TIMA]++;
				//DPRINTF("TIMA=%02x\n", ioregs[REG_TIMA]);
				if (gbhw->ioregs[REG_TIMA] == 0) {
					gbhw->ioregs[REG_TIMA] = gbhw->ioregs[REG_TMA];
					gbhw->ioregs[REG_IF] |= 0x04;
					DPRINTF("timer_interrupt\n");
				}
			}
		}
		cycles_total += cycles;
	}

	return cycles_total;
}
