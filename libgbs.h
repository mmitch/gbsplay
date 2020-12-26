/*
 * gbsplay is a Gameboy sound player
 *
 * 2020 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *             Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#ifndef _LIBGBS_H_
#define _LIBGBS_H_

/***
 ***  THIS IS THE OFFICIAL EXTERNAL API, DON'T MAKE ANY INCOMPATIBLE CHANGES!
 ***/

#include <inttypes.h>
#include "common.h"

// FIXME: call everything (structs, enums, types, functions) "libgbs*" instead "gbs*"
//        to prevent internal/external namespace clashes?

struct gbs;
struct gbs_output_buffer;

typedef void (*gbs_sound_cb)(struct gbs_output_buffer *buf, void *priv);
typedef long (*gbs_nextsubsong_cb)(struct gbs *gbs, void *priv);

struct gbs_output_buffer {
	int16_t *data;
	long bytes;
	long pos;
};

struct gbs_channel_status {
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
	struct gbs_channel_status ch[4];
};

enum gbs_filter_type {
	FILTER_OFF,
	FILTER_DMG,
	FILTER_CGB,
};

struct gbs *gbs_open(const char *name);
struct gbs *gbs_open_mem(const char *name, char *buf, size_t size);
void gbs_configure(struct gbs *gbs, long subsong, long subsong_timeout, long silence_timeout, long subsong_gap, long fadeout);
void gbs_configure_channels(struct gbs *gbs, long mute_0, long mute_1, long mute_2, long mute_3);
void gbs_configure_output(struct gbs *gbs, struct gbs_output_buffer *buf, long rate);
const uint8_t *gbs_get_bootrom();
const char *gbs_get_title(struct gbs *gbs);
const char *gbs_get_author(struct gbs *gbs);
const char *gbs_get_copyright(struct gbs *gbs);
long gbs_init(struct gbs *gbs, long subsong);
uint8_t gbs_io_peek(struct gbs *gbs, uint16_t addr);
const struct gbs_status* gbs_get_status(struct gbs *gbs);
long gbs_step(struct gbs *gbs, long time_to_work);
void gbs_set_nextsubsong_cb(struct gbs *gbs, gbs_nextsubsong_cb cb, void *priv);
void gbs_set_sound_callback(struct gbs *gbs, gbs_sound_cb fn, void *priv);
long gbs_set_filter(struct gbs *gbs, enum gbs_filter_type type);
void gbs_print_info(struct gbs *gbs, long verbose);
long gbs_toggle_mute(struct gbs *gbs, long channel);
void gbs_close(struct gbs *gbs);
long gbs_write(struct gbs *gbs, char *name);
void gbs_write_rom(struct gbs *gbs, FILE *out, const uint8_t *logo_data);

#endif
