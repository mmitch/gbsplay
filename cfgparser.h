/* $Id: cfgparser.h,v 1.6 2004/03/16 14:07:18 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 */

#ifndef _CFGPARSER_H_
#define _CFGPARSER_H_

#define CFG_ENDIAN_BE 0
#define CFG_ENDIAN_LE 1
#define CFG_ENDIAN_NE 2

typedef void regparm (*cfg_parse_fn)(void *ptr);

struct cfg_option {
	char *name;
	void *ptr;
	cfg_parse_fn parse_fn;
};

void regparm cfg_int(void *ptr);
void regparm cfg_endian(void *ptr);
void regparm cfg_parse(char *fname, struct cfg_option *options);

#endif
