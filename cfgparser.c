/* $Id: cfgparser.c,v 1.10 2004/03/21 02:46:14 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003-2004 (C) by Tobias Diedrich <ranma@gmx.at>
 *                  Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>

#include "common.h"
#include "cfgparser.h"

static int cfg_line, cfg_char;
static FILE *cfg_file;

static int nextchar_state;

static regparm char nextchar(void)
{
	int c;

	do {
		if ((c = fgetc(cfg_file)) == EOF) return 0;

		if (c == '\n') {
			cfg_char = 0;
			cfg_line++;
		} else cfg_char++;

		switch (nextchar_state) {
		case 0:
			if (c == '\\') nextchar_state = 1;
			else if (c == '#') nextchar_state = 2;
			break;
		case 1:
			nextchar_state = 0;
			if (c == 'n') c = '\n';
			break;
		case 2:
			if (c == 0 || c == '\n') nextchar_state = 0;
			break;
		}
	} while (nextchar_state != 0);

	return c;
}

static int state;
static int nextstate;
static int c;
static char *filename;

static regparm void err_expect(char *s)
{
	fprintf(stderr, _("'%s' expected at %s line %d char %d.\n"),
	        s, filename, cfg_line, cfg_char);
	c = nextchar();
	state = 0;
	nextstate = 1;
}

regparm void cfg_endian(void *ptr)
{
	c = tolower(c);
	if (c != 'b' && c != 'l' && c!= 'n') {
		err_expect("[bln]");
		return;
	}

	switch (c) {
	case 'b': *((int*)ptr) = CFG_ENDIAN_BE; break;
	case 'l': *((int*)ptr) = CFG_ENDIAN_LE; break;
	default: *((int*)ptr) = CFG_ENDIAN_NE; break;
	}

	c = nextchar();
	state = 0;
	nextstate = 1;
}

regparm void cfg_string(void *ptr)
{
	char s[200];
	unsigned int n = 0;

	if (!isalpha(c) && c != '-' && c != '_') {
		err_expect("[a-zA-Z_-]");
		return;
	}
	do {
		s[n++] = c;
		c = nextchar();
	} while ((isalnum(c) || c == '-' || c == '_') &&
	         n < (sizeof(s)-1));
	s[n] = 0;

	*((char**)ptr) = strdup(s);

	state = 0;
	nextstate = 1;
}

regparm void cfg_int(void *ptr)
{
	char num[20];
	unsigned int n = 0;

	if (!isdigit(c)) {
		err_expect("[0-9]");
		return;
	}
	do {
		num[n++] = c;
		c = nextchar();
	} while (isdigit(c) &&
	         n < (sizeof(num)-1));
	num[n] = 0;

	*((int*)ptr) = atoi(num);

	state = 0;
	nextstate = 1;
}

regparm void cfg_parse(char *fname, struct cfg_option *options)
{
	filename = fname;
	cfg_file = fopen(fname, "r");
	if (cfg_file == NULL) return;

	nextchar_state = 0;
	state = 0;
	nextstate = 1;
	cfg_line = 1;
	cfg_char = 0;
	c = nextchar();

	do {
		char option[200];
		unsigned int n;
		switch (state) {
		case 0:
			if (isspace(c))
				while (isspace(c = nextchar()));
			state = nextstate;
			break;
		case 1:
			n = 0;
			if (!isalpha(c)) {
				err_expect("[a-zA-Z]");
				break;
			}
			do {
				option[n++] = c;
				c = nextchar();
			} while ((isalnum(c) ||
			          c == '-' ||
			          c == '_') &&
			          n < (sizeof(option)-1));
			option[n] = 0;
			state = 0;
			nextstate = 2;
			break;
		case 2:
			if (c != '=') {
				err_expect("=");
				break;
			}
			state = 0;
			nextstate = 3;
			c = nextchar();
			break;
		case 3:
			n=0;
			while (options[n].name != NULL &&
			       strcmp(options[n].name, option) != 0) n++;
			if (options[n].parse_fn)
				options[n].parse_fn(options[n].ptr);
			else {
				fprintf(stderr, _("Unknown option %s at %s line %d.\n"), option, fname, cfg_line);
				while ((c = nextchar()) != '\n' && c != 0);
				state = 0;
				nextstate = 1;
			}
			c = nextchar();
			break;
		}
	} while (c != 0);

	fclose(cfg_file);
}

regparm char* get_userconfig(const char* cfgfile)
{
	char *homedir, *usercfg;
	int length;

	homedir = getenv("HOME");
	length  = strlen(homedir) + strlen(cfgfile) + 2;
	usercfg = malloc(length);
	snprintf(usercfg, length, "%s/%s", homedir, cfgfile);

	free(homedir);
	return usercfg;
}

