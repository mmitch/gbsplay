#ifndef _COMMON_H_
#define _COMMON_H_

#include "config.h"

#define TEXTDOMAIN "gbsplay"
#define N_(x) x

#if HAVE_LOCALE_H == 1 && HAVE_LIBINTL_H == 1

#  include <locale.h>
#  include <libintl.h>
#  define _(x) gettext(x)

static inline void i18n_init(void)
{
	setlocale(LC_ALL, "");
	bindtextdomain(TEXTDOMAIN, LOCALE_PREFIX);
	textdomain(TEXTDOMAIN);
}

#else

#  define _(x) (x)
static inline void i18n_init(void) {}

#endif

#endif
