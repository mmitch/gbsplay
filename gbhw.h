/*
 * gbsplay is a Gameboy sound player
 *
 * 2003-2020 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                  Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#ifndef _GBHW_H_
#define _GBHW_H_

#include <inttypes.h>
#include "common.h"
#include "gbcpu.h"

#define GBHW_CLOCK 4194304

#define GBHW_CFG_FILTER_OFF "off"
#define GBHW_CFG_FILTER_DMG "dmg"
#define GBHW_CFG_FILTER_CGB "cgb"

#define GBHW_FILTER_CONST_OFF 1.0
/* From blargg's "Game Boy Sound Operation" doc */
#define GBHW_FILTER_CONST_DMG 0.999958
#define GBHW_FILTER_CONST_CGB 0.998943

#define GBHW_HIRAM_SIZE 0x80
#define GBHW_INTRAM_SIZE 0x200
#define GBHW_EXTRAM_SIZE 0x200
#define GBHW_IOREGS_SIZE 0x80

#define GBHW_BOOT_ROM_SIZE 256

struct gbhw_buffer {
	int16_t *data;
	long pos;
	long l_lvl;
	long r_lvl;
	long l_cap;
	long r_cap;
	long bytes;
	long samples;
	long cycles;
};

struct gbhw_channel {
	long mute;
	long running;
	long master;
	long leftgate;
	long rightgate;
	long lvl;
	long volume;
	long env_volume;
	long env_dir;
	long env_tc;
	long env_ctr;
	long sweep_dir;
	long sweep_tc;
	long sweep_ctr;
	long sweep_shift;
	long len;
	long len_enable;
	long len_gate;
	long div_tc;
	long div_tc_shadow;
	long div_ctr;
	long duty_tc;
	long duty_ctr;
};

typedef void (*gbhw_callback_fn)(struct gbhw_buffer *buf, void *priv);
typedef void (*gbhw_iocallback_fn)(long cycles, uint32_t addr, uint8_t valu, void *priv);
typedef void (*gbhw_stepcallback_fn)(const long cycles, const struct gbhw_channel[], void *priv);

struct gbhw {
	uint8_t *rom;
	long rombank;
	long lastbank;
	long apu_on;
	long io_written;

	long lminval, lmaxval, rminval, rmaxval;
	double filter_constant;
	int filter_enabled;
	long cap_factor;

	long master_volume;
	long master_fade;
	long master_dstvol;
	long sample_rate;
	long update_level;
	long sequence_ctr;
	long halted_noirq_cycles;

	long vblankctr;
	long timertc;
	long timerctr;

	long sum_cycles;

	long pause_output;
	long rom_lockout;

	gbhw_callback_fn callback;
	void *callbackpriv;
	struct gbhw_buffer *soundbuf; /* externally visible output buffer */
	struct gbhw_buffer *impbuf;   /* internal impulse output buffer */

	gbhw_iocallback_fn iocallback;
	void *iocallback_priv;

	gbhw_stepcallback_fn stepcallback;
	void *stepcallback_priv;

	uint32_t tap1;
	uint32_t tap2;
	uint32_t lfsr;

	long long sound_div_tc;
	long main_div;
	long sweep_div;

	long ch3pos;
	long last_l_value, last_r_value;
	long ch3_next_nibble;

	struct gbcpu gbcpu;

	struct gbhw_channel ch[4];

	uint8_t hiram[GBHW_HIRAM_SIZE];
	uint8_t intram[GBHW_INTRAM_SIZE];
	uint8_t extram[GBHW_EXTRAM_SIZE];
	uint8_t ioregs[GBHW_IOREGS_SIZE];

	uint8_t boot_rom[GBHW_BOOT_ROM_SIZE];
};

struct gbhw* gbhw_create();
void gbhw_handle_init(struct gbhw *gbhw);
void gbhw_free(struct gbhw *gbhw);

void gbhw_setcallback(struct gbhw *gbhw, gbhw_callback_fn fn, void *priv);
void gbhw_setiocallback(struct gbhw *gbhw, gbhw_iocallback_fn fn, void *priv);
void gbhw_setstepcallback(struct gbhw *gbhw, gbhw_stepcallback_fn fn, void *priv);
long gbhw_setfilter(struct gbhw *gbhw, const char *type);
void gbhw_setrate(struct gbhw *gbhw, long rate);
void gbhw_setbuffer(struct gbhw *gbhw, struct gbhw_buffer *buffer);
void gbhw_init(struct gbhw *gbhw, uint8_t *rombuf, uint32_t size);
void gbhw_enable_bootrom(struct gbhw *gbhw, const uint8_t *rombuf);
void gbhw_pause(struct gbhw *gbhw, long new_pause);
void gbhw_master_fade(struct gbhw *gbhw, long speed, long dstvol);
void gbhw_getminmax(struct gbhw *gbhw, int16_t *lmin, int16_t *lmax, int16_t *rmin, int16_t *rmax);
long gbhw_step(struct gbhw *gbhw, long time_to_work);
uint8_t gbhw_io_peek(struct gbhw *gbhw, uint16_t addr);  /* unmasked peek */
void gbhw_io_put(struct gbhw *gbhw, uint16_t addr, uint8_t val);

#endif
