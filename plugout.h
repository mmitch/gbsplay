#ifndef _PLUGOUT_H_
#define _PLUGOUT_H_

enum plugout_endian {
	PLUGOUT_ENDIAN_BIG,
	PLUGOUT_ENDIAN_LITTLE,
	PLUGOUT_ENDIAN_NATIVE
};

typedef long    regparm (*plugout_open_fn )(enum plugout_endian endian, long rate);
typedef ssize_t regparm (*plugout_write_fn)(const void *buf, size_t count);
typedef void    regparm (*plugout_close_fn)(void);

#define PLUGOUT_USES_STDOUT	1

struct output_plugin {
	char	*name;
	char	*description;
	long	flags;
	plugout_open_fn  open;
	plugout_write_fn write;
	plugout_close_fn close;
};

regparm void plugout_list_plugins(void);
regparm struct output_plugin* plugout_select_by_name(char *name);

#endif
