/* $Id: common.h,v 1.9 2004/10/23 21:19:17 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003,2004 (C) by Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#include "config.h"

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

static inline char* _(const char *msgid)
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

#    define _(x) gettext(x)

static inline void i18n_init(void)
{
	setlocale(LC_ALL, "");
	bindtextdomain(TEXTDOMAIN, LOCALE_PREFIX);
	textdomain(TEXTDOMAIN);
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

static inline int is_le_machine() {
	return false;
}

static inline int is_be_machine() {
	return true;
}

#else

static inline int is_le_machine() {
	return true;
}

static inline int is_be_machine() {
	return false;
}

#endif

#endif
