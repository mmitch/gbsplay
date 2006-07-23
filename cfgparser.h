/* $Id: cfgparser.h,v 1.13 2006/07/23 13:28:46 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003-2005 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                  Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL.
 */

#ifndef _CFGPARSER_H_
#define _CFGPARSER_H_

typedef void regparm (*cfg_parse_fn)(void *ptr);

struct cfg_option {
	char *name;
	void *ptr;
	cfg_parse_fn parse_fn;
};

void  regparm cfg_string(void *ptr);
void  regparm cfg_long(void *ptr);
void  regparm cfg_endian(void *ptr);
void  regparm cfg_parse(const char *fname, const struct cfg_option *options);
/*@null@*/ /*@only@*/ char* regparm get_userconfig(const char* cfgfile);

#endif
