/* $Id: cfgparser.h,v 1.9 2004/03/21 02:46:14 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003-2004 (C) by Tobias Diedrich <ranma@gmx.at>
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
void  regparm cfg_int(void *ptr);
void  regparm cfg_endian(void *ptr);
void  regparm cfg_parse(char *fname, struct cfg_option *options);
char* regparm get_userconfig(const char* cfgfile);

#endif
