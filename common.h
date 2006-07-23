/* $Id: common.h,v 1.12 2006/07/23 13:28:46 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003-2005 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 * Licensed under GNU GPL.
 */

#ifndef _COMMON_H_
#define _COMMON_H_
/*@-onlytrans@*/

#include "config.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>

#ifndef true
#define true (0==0)
#define false (!true)
#endif

#define TEXTDOMAIN "gbsplay"
#define N_(x) x

#if USE_REGPARM == 1
#  define regparm __attribute__((regparm(3)))
#else
#  define regparm
#endif

#if USE_I18N == 1

#  include <locale.h>
#  include <libintl.h>

#  if GBS_PRESERVE_TEXTDOMAIN == 1

static /*@dependent@*/ inline char* _(const char *msgid)
{
	char *olddomain = textdomain(NULL);
	char *olddir = bindtextdomain(olddomain, NULL);
	char *res;

	bindtextdomain(TEXTDOMAIN, LOCALE_PREFIX);
	res = dgettext(TEXTDOMAIN, msgid);
	bindtextdomain(olddomain, olddir);

	return res;
}

#  else

static inline /*@dependent@*/ char* _(const char *msgid)
{
	return gettext(msgid);
}

static inline void i18n_init(void)
{
	if (setlocale(LC_ALL, "") == NULL) {
		fprintf(stderr, "setlocale() failed\n");
	}
	if (bindtextdomain(TEXTDOMAIN, LOCALE_PREFIX) == NULL) {
		fprintf(stderr, "bindtextdomain() failed: %s\n", strerror(errno));
	}
	if (textdomain(TEXTDOMAIN) == NULL) {
		fprintf(stderr, "textdomain() failed: %s\n", strerror(errno));
	}
}

#  endif

#else

#  define _(x) (x)
static inline void i18n_init(void) {}

#endif

#include <sys/param.h>

#ifndef BYTE_ORDER

#  define BIG_ENDIAN 1
#  define LITTLE_ENDIAN 2

#  ifdef _BIG_ENDIAN
#    define BYTE_ORDER BIG_ENDIAN
#  endif

#  ifdef _LITTLE_ENDIAN
#    define BYTE_ORDER LITTLE_ENDIAN
#  endif

#endif /* BYTE_ORDER */

#if !BYTE_ORDER || !BIG_ENDIAN || !LITTLE_ENDIAN
#  error endian defines missing
#endif

#if BYTE_ORDER == BIG_ENDIAN

static inline long is_le_machine() {
	return false;
}

static inline long is_be_machine() {
	return true;
}

#else

static inline long is_le_machine() {
	return true;
}

static inline long is_be_machine() {
	return false;
}

#endif

/*@=onlytrans@*/
#endif
