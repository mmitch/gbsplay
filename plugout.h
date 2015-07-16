#ifndef _PLUGOUT_H_
#define _PLUGOUT_H_

#include <stdint.h>

#include "config.h"

#if PLUGOUT_DSOUND == 1
#  define PLUGOUT_DEFAULT "dsound"
#elif PLUGOUT_PULSE == 1
#  define PLUGOUT_DEFAULT "pulse"
#elif PLUGOUT_ALSA == 1
#  define PLUGOUT_DEFAULT "alsa"
#else
#  define PLUGOUT_DEFAULT "oss"
#endif

enum plugout_endian {
	PLUGOUT_ENDIAN_BIG,
	PLUGOUT_ENDIAN_LITTLE,
	PLUGOUT_ENDIAN_NATIVE
};

typedef long    regparm (*plugout_open_fn )(enum plugout_endian endian, long rate);
typedef int     regparm (*plugout_skip_fn )(int subsong);
typedef int     regparm (*plugout_pause_fn)(int pause);
typedef int     regparm (*plugout_io_fn   )(long cycles, uint32_t addr, uint8_t val);
typedef ssize_t regparm (*plugout_write_fn)(const void *buf, size_t count);
typedef void    regparm (*plugout_close_fn)(void);

#define PLUGOUT_USES_STDOUT	1

struct output_plugin {
	char	*name;
	char	*description;
	long	flags;
	plugout_open_fn  open;
	plugout_skip_fn  skip;
	plugout_pause_fn pause;
	plugout_io_fn    io;
	plugout_write_fn write;
	plugout_close_fn close;
};

regparm void plugout_list_plugins(void);
regparm /*@null@*/ /*@temp@*/ const struct output_plugin* plugout_select_by_name(const char *name);

#endif
