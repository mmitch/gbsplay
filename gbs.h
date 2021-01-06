/*
 * gbsplay is a Gameboy sound player
 *
 * 2003-2020 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                  Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#ifndef _GBS_H_
#define _GBS_H_

#include <inttypes.h>
#include "common.h"
#include "gbhw.h"

#define GBS_LEN_SHIFT	10
#define GBS_LEN_DIV	(1 << GBS_LEN_SHIFT)

struct gbs;
struct gbs_output_buffer;

typedef void (*gbs_sound_cb)(struct gbs_output_buffer *buf, void *priv);
typedef long (*gbs_nextsubsong_cb)(struct gbs *gbs, void *priv);

struct gbs_output_buffer {
	int16_t *data;
	long bytes;
	long pos;
};

struct gbs_status_ch {
	long mute;
	long vol;
	long div_tc;
};

struct gbs_status {
	char *songtitle;
	int subsong;
	uint32_t subsong_len;
	uint8_t songs;
	uint8_t defaultsong;
	long lvol;
	long rvol;
	long long ticks;
	struct gbs_status_ch ch[4];
};

struct gbs *gbs_open(const char *name);
struct gbs *gbs_open_mem(const char *name, char *buf, size_t size);
void gbs_configure(struct gbs *gbs, long subsong, long subsong_timeout, long silence_timeout, long subsong_gap, long fadeout);
void gbs_configure_channels(struct gbs *gbs, long mute_0, long mute_1, long mute_2, long mute_3);
void gbs_configure_output(struct gbs *gbs, struct gbs_output_buffer *buf, long rate);
long gbs_init(struct gbs *gbs, long subsong);
uint8_t gbs_io_peek(struct gbs *gbs, uint16_t addr);
const struct gbs_status* gbs_get_status(struct gbs *gbs);
long gbs_step(struct gbs *gbs, long time_to_work);
void gbs_set_nextsubsong_cb(struct gbs *gbs, gbs_nextsubsong_cb cb, void *priv);
void gbs_set_sound_callback(struct gbs *gbs, gbs_sound_cb fn, void *priv);
long gbs_set_gbhw_filter(struct gbs *gbs, const char *type); // FIXME: don't export gbhw
void gbs_set_gbhw_io_callback(struct gbs *gbs, gbhw_iocallback_fn fn, void *priv); // FIXME: don't export gbhw
void gbs_set_gbhw_step_callback(struct gbs *gbs, gbhw_stepcallback_fn fn, void *priv); // FIXME: don't export gbhw
void gbs_print_info(struct gbs *gbs, long verbose);
long gbs_toggle_mute(struct gbs *gbs, long channel);
void gbs_close(struct gbs *gbs);
long gbs_write(struct gbs *gbs, char *name);
void gbs_write_rom(struct gbs *gbs, FILE *out, const uint8_t *logo_data);
const uint8_t *gbs_get_bootrom();
const char *gbs_get_title(struct gbs *gbs);
const char *gbs_get_author(struct gbs *gbs);
const char *gbs_get_copyright(struct gbs *gbs);

#endif
