#ifndef _CFGPARSER_H_
#define _CFGPARSER_H_

typedef void (*cfg_parse_fn)(void *ptr);

struct cfg_option {
	char *name;
	void *ptr;
	cfg_parse_fn parse_fn;
};

void cfg_int(void *ptr);
void cfg_parse(char *fname, struct cfg_option *options);

#endif
