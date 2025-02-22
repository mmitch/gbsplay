/*
 * gbsplay is a Gameboy sound player
 *
 * 2003-2025 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                  Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "common.h"
#include "cfgparser.h"
#include "plugout.h"
#include "test.h"

typedef void (*cfg_parse_fn)(void* const ptr);

struct cfg_option {
	const char* const name;
	void* const ptr;
	const cfg_parse_fn parse_fn;
};

/* default values */

struct player_cfg cfg = {
	.fadeout = 3,
	.filter_type = CFG_FILTER_DMG,
	.loop_mode = LOOP_OFF,
	.rate = 44100,
	.refresh_delay = 33, // ms
	.requested_endian = PLUGOUT_ENDIAN_AUTOSELECT,
	.silence_timeout = 2,
	.sound_name = PLUGOUT_DEFAULT,
	.subsong_gap = 2,
	.subsong_timeout = 2*60,
	.verbosity = 3,
};

/* forward declarations needed by configuration directives */
static void cfg_string(void* const ptr);
static void cfg_long(void* const ptr);
static void cfg_bool_as_int(void* const ptr);
static void cfg_endian(void* const ptr);

/* configuration directives */
static const struct cfg_option options[] = {
	{ "endian", &cfg.requested_endian, cfg_endian },
	{ "fadeout", &cfg.fadeout, cfg_long },
	{ "filter_type", &cfg.filter_type, cfg_string },
	{ "loop", &cfg.loop_mode, cfg_bool_as_int },
	{ "output_plugin", &cfg.sound_name, cfg_string },
	{ "rate", &cfg.rate, cfg_long },
	{ "refresh_delay", &cfg.refresh_delay, cfg_long },
	{ "silence_timeout", &cfg.silence_timeout, cfg_long },
	{ "subsong_gap", &cfg.subsong_gap, cfg_long },
	{ "subsong_timeout", &cfg.subsong_timeout, cfg_long },
	{ "verbosity", &cfg.verbosity, cfg_long },
	/* playmode not implemented yet */
	{ NULL, NULL, NULL }
};

static long cfg_line, cfg_char;
static FILE *cfg_file;

static long nextchar_state;

static char nextchar(void)
{
	long c;

	assert(cfg_file != NULL);

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

	return (char)c;
}

static long state;
static long nextstate;
static long c;
static const char *filename;

static void err_expect(const char* const s)
{
	fprintf(stderr, _("'%s' expected at %s line %ld char %ld.\n"),
	        s, filename, cfg_line, cfg_char);
	c = nextchar();
	state = 0;
	nextstate = 1;
}

static void cfg_endian(void* const ptr)
{
	enum plugout_endian *endian = ptr;

	c = tolower(c);
	if (c != 'b' && c != 'l' && c!= 'n') {
		err_expect("[bln]");
		return;
	}

	switch (c) {
	case 'b': *endian = PLUGOUT_ENDIAN_BIG; break;
	case 'l': *endian = PLUGOUT_ENDIAN_LITTLE; break;
	default: *endian = PLUGOUT_ENDIAN_NATIVE; break;
	}

	c = nextchar();
	state = 0;
	nextstate = 1;
}

static void cfg_string(void * const ptr)
{
	char s[200];
	unsigned long n = 0;

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

static void cfg_long(void* const ptr)
{
	char num[20];
	unsigned long n = 0;

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

	*((long*)ptr) = atoi(num);

	state = 0;
	nextstate = 1;
}

static void cfg_bool_as_int(void* const ptr)
{
	char num[10];
	unsigned int tmp;
	unsigned long n = 0;

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

	tmp = atoi(num);
	*((int*)ptr) = tmp ? 1 : 0;

	state = 0;
	nextstate = 1;
}

void cfg_parse(const char* const fname)
{
	char option[200] = "";

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
		unsigned long n;
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
			option[n] = '\0';
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
				fprintf(stderr, _("Unknown option %s at %s line %ld.\n"), option, fname, cfg_line);
				while ((c = nextchar()) != '\n' && c != 0);
				state = 0;
				nextstate = 1;
			}
			c = nextchar();
			break;
		}
	} while (c != 0);

	(void)fclose(cfg_file);
}

char* get_userconfig(const char* const cfgfile)
{
	char *homedir, *usercfg;
	long length;

	homedir = getenv("HOME");
	if (!homedir || !cfgfile) return NULL;

	length  = strlen(homedir) + strlen(cfgfile) + 2;
	usercfg = malloc(length);
	if (usercfg == NULL) {
		fprintf(stderr, "%s\n", _("Memory allocation failed!"));
		return NULL;
	}
	(void)snprintf(usercfg, length, "%s/%s", homedir, cfgfile);

	return usercfg;
}

/********************* test helpers *********************/

#ifdef ENABLE_TEST

struct player_cfg initial_cfg;

#define ASSERT_STRUCT_EQUAL(fmt, field, a, b) ASSERT_EQUAL(#field " " fmt, (a).field, (b).field)

#define ASSERT_STRUCT_STRING_EQUAL(field, a, b) ASSERT_STRING_EQUAL(#field, (a).field, (b).field)

#define ASSERT_CFG_EQUAL(actual, expected) do { \
		ASSERT_STRUCT_EQUAL("%ld", fadeout,          actual, expected); \
		ASSERT_STRUCT_EQUAL("%d",  loop_mode,        actual, expected); \
		ASSERT_STRUCT_EQUAL("%ld", rate,             actual, expected); \
		ASSERT_STRUCT_EQUAL("%ld", refresh_delay,    actual, expected); \
		ASSERT_STRUCT_EQUAL("%d",  requested_endian, actual, expected); \
		ASSERT_STRUCT_EQUAL("%ld", silence_timeout,  actual, expected); \
		ASSERT_STRUCT_EQUAL("%ld", subsong_gap,      actual, expected); \
		ASSERT_STRUCT_EQUAL("%ld", subsong_timeout,  actual, expected); \
		ASSERT_STRUCT_EQUAL("%ld", verbosity,        actual, expected); \
		ASSERT_STRUCT_STRING_EQUAL(filter_type,      actual, expected); \
		ASSERT_STRUCT_STRING_EQUAL(sound_name,       actual, expected); \
} while(0)

test void save_initial_cfg() {
	initial_cfg = cfg;
	initial_cfg.filter_type = strdup(cfg.filter_type);
	initial_cfg.sound_name  = strdup(cfg.sound_name);
};

/************************* tests ************************/

test void test_parse_missing_config_file() {
	// given
	save_initial_cfg();

	// when
	cfg_parse("test/gbsplayrc-this-file-does-not-exist");

	// then
	ASSERT_CFG_EQUAL(cfg, initial_cfg);
}
TEST(test_parse_missing_config_file);

test void test_parse_empty_configuration() {
	// given
	save_initial_cfg();

	// when
	cfg_parse("test/gbsplayrc-empty");

	// then
	ASSERT_CFG_EQUAL(cfg, initial_cfg);
}
TEST(test_parse_empty_configuration);

test void test_parse_complete_configuration() {
	// given

	// when
	cfg_parse("test/gbsplayrc-full");

	// then
	ASSERT_EQUAL("fadeout %ld",         cfg.fadeout,          0L);
	ASSERT_EQUAL("loop_mode %d",        cfg.loop_mode,        LOOP_RANGE);
	ASSERT_EQUAL("rate %ld",            cfg.rate,             12345L);
	ASSERT_EQUAL("refresh_delay %ld",   cfg.refresh_delay,    987L);
	ASSERT_EQUAL("requested_endian %d", cfg.requested_endian, PLUGOUT_ENDIAN_LITTLE);
	ASSERT_EQUAL("silence_timeout %ld", cfg.silence_timeout,  19L);
	ASSERT_EQUAL("subsong_gap %ld",     cfg.subsong_gap,      23L);
	ASSERT_EQUAL("subsong_timeout %ld", cfg.subsong_timeout,  42L);
	ASSERT_EQUAL("verbosity   %ld"    , cfg.verbosity,        5L);
	ASSERT_STRING_EQUAL("filter_type",  cfg.filter_type,      CFG_FILTER_CGB);
	ASSERT_STRING_EQUAL("sound_name",   cfg.sound_name,       "altmidi");
}
TEST(test_parse_complete_configuration);

/* manpage says: an integer; 0 = false = no loop mode; everything else = true = range loop mode */
test void test_loop_0() {
	// given

	// when
	cfg_parse("test/gbsplayrc-loop-0");

	// then
	ASSERT_EQUAL("loop_mode %d", cfg.loop_mode, LOOP_OFF);
}
TEST(test_loop_0);

test void test_loop_1() {
	// given

	// when
	cfg_parse("test/gbsplayrc-loop-1");

	// then
	ASSERT_EQUAL("loop_mode %d", cfg.loop_mode, LOOP_RANGE);
}
TEST(test_loop_1);

test void test_loop_2() {
	// given

	// when
	cfg_parse("test/gbsplayrc-loop-2");

	// then
	ASSERT_EQUAL("loop_mode %d", cfg.loop_mode, LOOP_RANGE);
}
TEST(test_loop_2);

TEST_EOF;

#endif /* ENABLE_TEST */
