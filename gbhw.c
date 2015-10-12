/*
 * gbsplay is a Gameboy sound player
 *
 * 2003-2006,2008 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                       Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include "gbcpu.h"
#include "gbhw.h"
#include "impulse.h"

#define REG_IF 0x0f
#define REG_IE 0x7f /* Nominally 0xff, but we remap it to 0x7f internally. */

static uint8_t *rom;
static uint8_t intram[0x2000];
static uint8_t extram[0x2000];
static uint8_t ioregs[0x80];
static uint8_t hiram[0x80];
static long rombank = 1;
static long lastbank;
static long apu_on = 1;

static const uint8_t ioregs_ormask[sizeof(ioregs)] = {
	/* 0x00 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x10 */ 0x80, 0x3f, 0x00, 0xff, 0xbf,
	/* 0x15 */ 0xff, 0x3f, 0x00, 0xff, 0xbf,
	/* 0x1a */ 0x7f, 0xff, 0x9f, 0xff, 0xbf,
	/* 0x1f */ 0xff, 0xff, 0x00, 0x00, 0xbf,
	/* 0x24 */ 0x00, 0x00, 0x70, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};
static const uint8_t ioregs_initdata[sizeof(ioregs)] = {
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

struct gbhw_channel gbhw_ch[4];

static long lminval, lmaxval, rminval, rmaxval;

#define MASTER_VOL_MIN	0
#define MASTER_VOL_MAX	(256*256)
static long master_volume;
static long master_fade;
static long master_dstvol;

static const long vblanktc = 70256; /* ~59.7 Hz (vblankctr)*/
static long vblankctr = 70256;
static long timertc = 70256;
static long timerctr = 70256;

static const long msec_cycles = GBHW_CLOCK/1000;

static long sum_cycles;

static long pause_output = 0;

static gbhw_callback_fn callback;
static /*@null@*/ /*@dependent@*/ void *callbackpriv;
static /*@null@*/ /*@dependent@*/ struct gbhw_buffer *soundbuf = NULL; /* externally visible output buffer */
static /*@null@*/ /*@only@*/ struct gbhw_buffer *impbuf = NULL;   /* internal impulse output buffer */

static gbhw_iocallback_fn iocallback;
static /*@null@*/ /*@dependent@*/ void *iocallback_priv;

#define TAP1_15		0x4000;
#define TAP2_15		0x2000;
#define TAP1_7		0x0040;
#define TAP2_7		0x0020;

static uint32_t tap1 = TAP1_15;
static uint32_t tap2 = TAP2_15;
static uint32_t lfsr = 0xffffffff;

#define SOUND_DIV_MULT 0x10000LL

static long long sound_div_tc = 0;
static const long main_div_tc = 32;
static long main_div;
static const long sweep_div_tc = 256;
static long sweep_div;

static long ch3pos;

#define IMPULSE_WIDTH (1 << IMPULSE_W_SHIFT)
#define IMPULSE_N (1 << IMPULSE_N_SHIFT)
#define IMPULSE_N_MASK (IMPULSE_N - 1)

static regparm uint32_t rom_get(uint32_t addr)
{
//	DPRINTF("rom_get(%04x)\n", addr);
	return rom[addr & 0x3fff];
}

static regparm uint32_t rombank_get(uint32_t addr)
{
//	DPRINTF("rombank_get(%04x)\n", addr);
	return rom[(addr & 0x3fff) + 0x4000*rombank];
}

static regparm uint32_t io_get(uint32_t addr)
{
	if (addr >= 0xff80 && addr <= 0xfffe) {
		return hiram[addr & 0x7f];
	}
	if (addr >= 0xff10 &&
	           addr <= 0xff3f) {
		return ioregs[addr & 0x7f] | ioregs_ormask[addr & 0x7f];
	}
	if (addr == 0xff00) return 0;
	if (addr == 0xff70) {
		/* GBC ram bank switch */
		WARN_ONCE("ioread from SVBK (CGB mode) ignored.\n");
		return 0xff;
	}
	if (addr == 0xffff) return ioregs[0x7f];
	WARN_ONCE("ioread from 0x%04x unimplemented.\n", (unsigned int)addr);
	DPRINTF("io_get(%04x)\n", addr);
	return 0xff;
}

static regparm uint32_t intram_get(uint32_t addr)
{
//	DPRINTF("intram_get(%04x)\n", addr);
	return intram[addr & 0x1fff];
}

static regparm uint32_t extram_get(uint32_t addr)
{
//	DPRINTF("extram_get(%04x)\n", addr);
	return extram[addr & 0x1fff];
}

static regparm void rom_put(uint32_t addr, uint8_t val)
{
	if (addr >= 0x2000 && addr <= 0x3fff) {
		val &= 0x1f;
		rombank = val + (val == 0);
		if (rombank > lastbank) {
			WARN_ONCE("Bank %ld out of range (0-%ld)!\n", rombank, lastbank);
			rombank = lastbank;
		}
	} else {
		WARN_ONCE("rom write of %02x to %04x ignored\n", val, addr);
	}
}

static regparm void io_put(uint32_t addr, uint8_t val)
{
	long chn = (addr - 0xff10)/5;
	if (iocallback)
		iocallback(sum_cycles, addr, val, iocallback_priv);

	if (addr >= 0xff80 && addr <= 0xfffe) {
		hiram[addr & 0x7f] = val;
		return;
	}
	if (apu_on == 0 && addr >= 0xff10 && addr < 0xff26) {
		return;
	}
	ioregs[addr & 0x7f] = val;
	DPRINTF(" ([0x%04x]=%02x) ", addr, val);
	switch (addr) {
		case 0xff06:
		case 0xff07:
			timertc = (256-ioregs[0x06]) * (16 << (((ioregs[0x07]+3) & 3) << 1));
			if ((ioregs[0x07] & 0xf0) == 0x80) timertc /= 2;
//			printf("Callback rate set to %2.2fHz.\n", GBHW_CLOCK/(float)timertc);
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
				long duty_ctr = val >> 6;
				long len = val & 0x3f;

				gbhw_ch[chn].duty_ctr = dutylookup[duty_ctr];
				gbhw_ch[chn].duty_tc = gbhw_ch[chn].div_tc*gbhw_ch[chn].duty_ctr/8;
				gbhw_ch[chn].len = (64 - len)*2;

				break;
			}
		case 0xff12:
		case 0xff17:
		case 0xff21:
			{
				long vol = val >> 4;
				long envdir = (val >> 3) & 1;
				long envspd = val & 7;

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
				long div = ioregs[0x13 + 5*chn];

				div |= ((long)ioregs[0x14 + 5*chn] & 7) << 8;
				gbhw_ch[chn].div_tc = 2048 - div;
				gbhw_ch[chn].duty_tc = gbhw_ch[chn].div_tc*gbhw_ch[chn].duty_ctr/8;

				if (addr == 0xff13 ||
				    addr == 0xff18 ||
				    addr == 0xff1d) break;
			}
			gbhw_ch[chn].len_enable = (ioregs[0x14 + 5*chn] & 0x40) > 0;

//			printf(" ch%ld: vol=%02d envd=%ld envspd=%ld duty_ctr=%ld len=%03d len_en=%ld key=%04d gate=%ld%ld\n", chn, gbhw_ch[chn].volume, gbhw_ch[chn].env_dir, gbhw_ch[chn].env_tc, gbhw_ch[chn].duty_ctr, gbhw_ch[chn].len, gbhw_ch[chn].len_enable, gbhw_ch[chn].div_tc, gbhw_ch[chn].leftgate, gbhw_ch[chn].rightgate);
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
				long vol = (ioregs[0x1c] >> 5) & 3;
				gbhw_ch[2].volume = vol;
				break;
			}
		case 0xff1f:
			break;
		case 0xff22:
		case 0xff23:
			{
				long reg = ioregs[0x22];
				long shift = reg >> 4;
				long rate = reg & 7;
				gbhw_ch[3].div_ctr = 0;
				gbhw_ch[3].div_tc = 16 << shift;
				if (reg & 8) {
					tap1 = TAP1_7;
					tap2 = TAP2_7;
				} else {
					tap1 = TAP1_15;
					tap2 = TAP2_15;
				}
				if (rate) gbhw_ch[3].div_tc *= rate;
				else gbhw_ch[3].div_tc /= 2;
				if (addr == 0xff22) break;
				if (val & 0x80) {  /* trigger */
					lfsr = 0xffffffff;
				}
//				printf(" ch4: vol=%02d envd=%ld envspd=%ld duty_ctr=%ld len=%03d len_en=%ld key=%04d gate=%ld%ld\n", gbhw_ch[3].volume, gbhw_ch[3].env_dir, gbhw_ch[3].env_ctr, gbhw_ch[3].duty_ctr, gbhw_ch[3].len, gbhw_ch[3].len_enable, gbhw_ch[3].div_tc, gbhw_ch[3].leftgate, gbhw_ch[3].rightgate);
			}
			gbhw_ch[chn].len_enable = (ioregs[0x23] & 0x40) > 0;
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
			if (val & 0x80) {
				ioregs[0x26] = 0x80;
				apu_on = 1;
			} else {
				long i;
				for (i = 0xff10; i < 0xff26; i++) {
					io_put(i, 0);
				}
				apu_on = 0;
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
		case 0xffff:
			break;
		default:
			WARN_ONCE("iowrite to 0x%04x unimplemented (val=%02x).\n", addr, val);
			break;
	}
}

static regparm void intram_put(uint32_t addr, uint8_t val)
{
	intram[addr & 0x1fff] = val;
}

static regparm void extram_put(uint32_t addr, uint8_t val)
{
	extram[addr & 0x1fff] = val;
}

static regparm void gb_sound_sweep(void)
{
	long i;

	if (gbhw_ch[0].sweep_tc) {
		gbhw_ch[0].sweep_ctr--;
		if (gbhw_ch[0].sweep_ctr < 0) {
			long val = gbhw_ch[0].div_tc >> gbhw_ch[0].sweep_shift;

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

regparm void gbhw_master_fade(long speed, long dstvol)
{
	if (dstvol < MASTER_VOL_MIN) dstvol = MASTER_VOL_MIN;
	if (dstvol > MASTER_VOL_MAX) dstvol = MASTER_VOL_MAX;
	master_dstvol = dstvol;
	if (dstvol > master_volume)
		master_fade = speed;
	else master_fade = -speed;
}

#define GET_NIBBLE(p, n) ({ \
	long index = ((n) >> 1) & 0xf; \
	long shift = (~(n) & 1) << 2; \
	(((p)[index] >> shift) & 0xf); })

static regparm void gb_flush_buffer(void)
{
	long i;
	long overlap;
	long l_smpl, r_smpl;

	if (!callback)
		return;

	assert(soundbuf != NULL);
	assert(impbuf != NULL);

	/* integrate buffer */
	l_smpl = soundbuf->l_lvl;
	r_smpl = soundbuf->r_lvl;
	for (i=0; i<soundbuf->samples; i++) {
		l_smpl = l_smpl + impbuf->data[i*2  ];
		r_smpl = r_smpl + impbuf->data[i*2+1];
		soundbuf->data[i*2  ] = l_smpl * master_volume / MASTER_VOL_MAX;
		soundbuf->data[i*2+1] = r_smpl * master_volume / MASTER_VOL_MAX;
		if (l_smpl > lmaxval) lmaxval = l_smpl;
		if (l_smpl < lminval) lminval = l_smpl;
		if (r_smpl > rmaxval) rmaxval = r_smpl;
		if (r_smpl < rminval) rminval = r_smpl;
	}
	soundbuf->pos = soundbuf->samples;
	soundbuf->l_lvl = l_smpl;
	soundbuf->r_lvl = r_smpl;

	if (callback != NULL) callback(soundbuf, callbackpriv);

	overlap = impbuf->samples - soundbuf->samples;
	memmove(impbuf->data, impbuf->data+(2*soundbuf->samples), 4*overlap);
	memset(impbuf->data + 2*overlap, 0, impbuf->bytes - 4*overlap);
	assert(impbuf->bytes == impbuf->samples*4);
	assert(soundbuf->bytes == soundbuf->samples*4);
	memset(soundbuf->data, 0, soundbuf->bytes);
	soundbuf->pos = 0;

	impbuf->cycles -= (sound_div_tc * soundbuf->samples) / SOUND_DIV_MULT;
}

static regparm void gb_change_level(long l_ofs, long r_ofs)
{
	long pos;
	long imp_idx;
	long imp_l = -IMPULSE_WIDTH/2;
	long imp_r = IMPULSE_WIDTH/2;
	long i;
	const short *ptr = base_impulse;

	if (!callback)
		return;

	assert(impbuf != NULL);
	pos = (long)(impbuf->cycles * SOUND_DIV_MULT / sound_div_tc);
	imp_idx = (long)((impbuf->cycles << IMPULSE_N_SHIFT)*SOUND_DIV_MULT / sound_div_tc) & IMPULSE_N_MASK;
	assert(pos + imp_r < impbuf->samples);
	assert(pos + imp_l >= 0);

	ptr += imp_idx * IMPULSE_WIDTH;

	for (i=imp_l; i<imp_r; i++) {
		long bufi = pos + i;
		long impi = i + IMPULSE_WIDTH/2;
		impbuf->data[bufi*2  ] += ptr[impi] * l_ofs;
		impbuf->data[bufi*2+1] += ptr[impi] * r_ofs;
	}

	impbuf->l_lvl += l_ofs*256;
	impbuf->r_lvl += r_ofs*256;
}

static regparm void gb_sound(long cycles)
{
	long i, j;
	long l_lvl = 0, r_lvl = 0;
	static long old_l = 0, old_r = 0;
	static long next_nibble = 0;

	assert(impbuf != NULL);

	for (j=0; j<cycles; j++) {
		main_div++;
		impbuf->cycles++;
		if (impbuf->cycles*SOUND_DIV_MULT >= sound_div_tc*(impbuf->samples - IMPULSE_WIDTH/2))
			gb_flush_buffer();

		if (gbhw_ch[2].master) {
			gbhw_ch[2].div_ctr--;
			if (gbhw_ch[2].div_ctr <= 0) {
				long val = next_nibble;
				long old_l = gbhw_ch[2].l_lvl;
				long old_r = gbhw_ch[2].r_lvl;
				long l_diff, r_diff;
				long pos = ch3pos++;
				next_nibble = GET_NIBBLE(&ioregs[0x30], pos) * 2;
				gbhw_ch[2].div_ctr = gbhw_ch[2].div_tc*2;
				if (gbhw_ch[2].volume) {
					val = val >> (gbhw_ch[2].volume-1);
				} else val = 0;
				if (gbhw_ch[2].volume && !gbhw_ch[2].mute) {
					if (gbhw_ch[2].leftgate)
						gbhw_ch[2].l_lvl = val - 15;
					if (gbhw_ch[2].rightgate)
						gbhw_ch[2].r_lvl = val - 15;
				}
				l_diff = gbhw_ch[2].l_lvl - old_l;
				r_diff = gbhw_ch[2].r_lvl - old_r;
				gb_change_level(l_diff, r_diff);
			}
		}

		if (gbhw_ch[3].master) {
			static long val;
			gbhw_ch[3].div_ctr--;
			if (gbhw_ch[3].div_ctr <= 0) {
				long old_l = gbhw_ch[3].l_lvl;
				long old_r = gbhw_ch[3].r_lvl;
				long l_diff, r_diff;
				gbhw_ch[3].div_ctr = gbhw_ch[3].div_tc;
				lfsr = (lfsr << 1) | (((lfsr & tap1) > 0) ^ ((lfsr & tap2) > 0));
				val = gbhw_ch[3].volume * 2 * (!(lfsr & tap1));
				if (!gbhw_ch[3].mute) {
					if (gbhw_ch[3].leftgate)
						gbhw_ch[3].l_lvl = val - 15;
					if (gbhw_ch[3].rightgate)
						gbhw_ch[3].r_lvl = val - 15;
				}
				l_diff = gbhw_ch[3].l_lvl - old_l;
				r_diff = gbhw_ch[3].r_lvl - old_r;
				gb_change_level(l_diff, r_diff);
			}
		}

		if (main_div > main_div_tc) {
			main_div -= main_div_tc;

			for (i=0; i<2; i++) if (gbhw_ch[i].master) {
				long val = 2 * gbhw_ch[i].volume;
				if (gbhw_ch[i].div_ctr > gbhw_ch[i].duty_tc) {
					val = 0;
				}
				if (!gbhw_ch[i].mute) {
					if (gbhw_ch[i].leftgate)
						gbhw_ch[i].l_lvl = val - 15;
					if (gbhw_ch[i].rightgate)
						gbhw_ch[i].r_lvl = val - 15;
				}
				gbhw_ch[i].div_ctr--;
				if (gbhw_ch[i].div_ctr <= 0) {
					gbhw_ch[i].div_ctr = gbhw_ch[i].div_tc;
				}
			}
			for (i=0; i<2; i++) {
				l_lvl += gbhw_ch[i].l_lvl;
				r_lvl += gbhw_ch[i].r_lvl;
			}

			if (l_lvl != old_l || r_lvl != old_r) {
				gb_change_level(l_lvl - old_l, r_lvl - old_r);
				old_l = l_lvl;
				old_r = r_lvl;
			}

			sweep_div += 1;
			if (sweep_div >= sweep_div_tc) {
				sweep_div = 0;
				gb_sound_sweep();
			}
		}
	}
}

regparm void gbhw_setcallback(gbhw_callback_fn fn, void *priv)
{
	callback = fn;
	callbackpriv = priv;
}

regparm void gbhw_setiocallback(gbhw_iocallback_fn fn, void *priv)
{
	iocallback = fn;
	iocallback_priv = priv;
}

static regparm void gbhw_impbuf_reset(struct gbhw_buffer *impbuf)
{
	assert(sound_div_tc != 0);
	impbuf->cycles = (long)(sound_div_tc * IMPULSE_WIDTH/2 / SOUND_DIV_MULT);
	impbuf->l_lvl = 0;
	impbuf->r_lvl = 0;
	memset(impbuf->data, 0, impbuf->bytes);
}

regparm void gbhw_setbuffer(struct gbhw_buffer *buffer)
{
	soundbuf = buffer;
	soundbuf->samples = soundbuf->bytes / 4;

	if (impbuf) free(impbuf);
	impbuf = malloc(sizeof(*impbuf) + (soundbuf->samples + IMPULSE_WIDTH + 1) * 4);
	if (impbuf == NULL) {
		fprintf(stderr, "%s", _("Memory allocation failed!\n"));
		return;
	}
	memset(impbuf, 0, sizeof(*impbuf));
	impbuf->data = (void*)(impbuf+1);
	impbuf->samples = soundbuf->samples + IMPULSE_WIDTH + 1;
	impbuf->bytes = impbuf->samples * 4;
	gbhw_impbuf_reset(impbuf);
}

regparm void gbhw_setrate(long rate)
{
	sound_div_tc = GBHW_CLOCK*SOUND_DIV_MULT/rate;
}

regparm void gbhw_getminmax(int16_t *lmin, int16_t *lmax, int16_t *rmin, int16_t *rmax)
{
	if (lminval == INT_MAX) return;
	*lmin = lminval;
	*lmax = lmaxval;
	*rmin = rminval;
	*rmax = rmaxval;
	lminval = rminval = INT_MAX;
	lmaxval = rmaxval = INT_MIN;
}

/*
 * Initialize Gameboy hardware emulation.
 * The size should be a multiple of 0x4000,
 * so we don't need range checking in rom_get and
 * rombank_get.
 */
regparm void gbhw_init(uint8_t *rombuf, uint32_t size)
{
	long i;
	int mute_tmp[4];

	for (i=0; i<4; i++)
		mute_tmp[i] = gbhw_ch[i].mute;
	if (impbuf)
		gbhw_impbuf_reset(impbuf);
	rom = rombuf;
	lastbank = ((size + 0x3fff) / 0x4000) - 1;
	rombank = 1;
	master_volume = MASTER_VOL_MAX;
	master_fade = 0;
	if (soundbuf) {
		soundbuf->pos = 0;
		soundbuf->l_lvl = 0;
		soundbuf->r_lvl = 0;
	}
	lminval = rminval = INT_MAX;
	lmaxval = rmaxval = INT_MIN;
	for (i=0; i<4; i++) {
		gbhw_ch[i].duty_ctr = 4;
		gbhw_ch[i].div_tc = 1;
		gbhw_ch[i].master = 1;
		gbhw_ch[i].mute = mute_tmp[i];
	}
	memset(extram, 0, sizeof(extram));
	memset(intram, 0, sizeof(intram));
	memset(hiram, 0, sizeof(hiram));
	memset(ioregs, 0, sizeof(ioregs));
	for (i=0x10; i<0x40; i++) {
		io_put(0xff00 + i, ioregs_initdata[i]);
	}

	sum_cycles = 0;

	gbcpu_init();
	gbcpu_addmem(0x00, 0x3f, rom_put, rom_get);
	gbcpu_addmem(0x40, 0x7f, rom_put, rombank_get);
	gbcpu_addmem(0xa0, 0xbf, extram_put, extram_get);
	gbcpu_addmem(0xc0, 0xfe, intram_put, intram_get);
	gbcpu_addmem(0xff, 0xff, io_put, io_get);
}

/* internal for gbs.c, not exported from libgbs */
regparm void gbhw_io_put(uint16_t addr, uint8_t val) {
	if (addr != 0xffff && (addr < 0xff00 || addr > 0xff7f))
		return;
	io_put(addr, val);
}

/* unmasked peek used by gbsplay.c to print regs */
regparm uint8_t gbhw_io_peek(uint16_t addr)
{
	if (addr >= 0xff10 && addr <= 0xff3e) {
		return ioregs[addr & 0x7f];
	}
	return 0xff;
}


regparm void gbhw_check_if(void)
{
	uint8_t mask = 0x01; /* lowest bit is highest priority irq */
	uint8_t vec = 0x40;
	if (!gbcpu_if) {
		/* interrupts disabled */
		return;
	}
	while (mask <= 0x10) {
		if (ioregs[REG_IF] & ioregs[REG_IE] & mask) {
			ioregs[REG_IF] &= ~mask;
			gbcpu_intr(vec);
			break;
		}
		vec += 0x08;
		mask <<= 1;
	}
}

/**
 * @param time_to_work  emulated time in milliseconds
 * @return  elapsed cpu cycles
 */
regparm long gbhw_step(long time_to_work)
{
	long cycles_total = 0;

	if (pause_output) {
		(void)usleep(time_to_work*1000);
		return 0;
	}

	time_to_work *= msec_cycles;
	
	while (cycles_total < time_to_work) {
		long maxcycles = time_to_work - cycles_total;
		long cycles = 0;

		if (vblankctr > 0 && vblankctr < maxcycles) maxcycles = vblankctr;
		if (timerctr > 0 && timerctr < maxcycles) maxcycles = timerctr;

		while (cycles < maxcycles) {
			long step;
			gbhw_check_if();
			step = gbcpu_step();
			if (step < 0) return step;
			cycles += step;
			sum_cycles += step;
			gb_sound(step);
		}

		if (vblankctr > 0) vblankctr -= cycles;
		if (vblankctr <= 0) {
			vblankctr += vblanktc;
			ioregs[REG_IF] |= 0x01;
		}
		if (timerctr > 0) timerctr -= cycles;
		if (timerctr <= 0 && (ioregs[0x07] & 4)) {
			timerctr += timertc;
			ioregs[REG_IF] |= 0x04;
		}
		cycles_total += cycles;
	}

	return cycles_total;
}

regparm void gbhw_pause(long new_pause)
{
	pause_output = new_pause != 0;
}
