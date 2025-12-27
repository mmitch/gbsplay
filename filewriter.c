/*
 * gbsplay is a Gameboy sound player
 *
 * write audio data to file
 *
 * 2006-2025 (C) by Christian Garbs <mitch@cgarbs.de>
 *                  Vegard Nossum
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "filewriter.h"
#include "playercfg.h"
#include "test.h"

#define FILENAME_SIZE 256

// set lower filename size used in unit tests to easier check handling of oversized strings
#ifdef TEST
#undef  FILENAME_SIZE
#define FILENAME_SIZE 23
#endif

static char filename[FILENAME_SIZE];

int expand_filename(const char* const filename_template, const char* const extension, const int subsong) {
	char* const last = filename + FILENAME_SIZE - 1;
	const char *src;
	char *dst;

	for (src = filename_template, dst = filename; *src != 0; src++) {
		if (*src == '%') {
			switch (*(++src)) {
			case '%': // %% -> literal %
				*dst++ = '%';
				break;

			case 's': // %s -> subsong number
				dst += snprintf(dst, last + 1 - dst, "%d", subsong + 1);
				break;

			case 'S': // %S -> subsong number with leading zeroes
				dst += snprintf(dst, last + 1 - dst, "%03d", subsong + 1);
				break;

			case 'e': // %e -> filename extension
				dst += snprintf(dst, last + 1 - dst, "%s", extension);
				break;

			default:
				fprintf(stderr, _("Unknown placeholder %%%c was not expanded.\n"), *src);
				dst += snprintf(dst, last + 1 - dst, "%%%c", *src);
				break;
			}
		} else {
			*dst++ = *src;
		}
	}

	if (dst <= last) {
		*dst = 0;
	} else {
		fprintf(stderr, "%s\n", _("Output filename too long!"));
		*(last) = 0;
	}

	return dst - filename;
}

FILE* file_open(const char* const extension, const int subsong) {
	FILE* file = NULL;

	if (expand_filename(cfg.output_filename, extension, subsong) >= FILENAME_SIZE)
		goto error;

	if ((file = fopen(filename, "wb")) == NULL)
		goto error;

	return file;

error:
	if (file != NULL) {
		fclose(file);
	}
	return NULL;
}

/************************* tests ************************/

#ifdef ENABLE_TEST

struct player_cfg cfg;

test void test_expand_filename_default_template_ok(void) {
	// given

	// when
	int chars = expand_filename("gbsplay-%s.%e", "wav", 0);

	// then
	ASSERT_EQUAL("chars written %d", chars, 13);
	ASSERT_STRING_EQUAL("filename %s", filename, "gbsplay-1.wav");
}
TEST(test_expand_filename_default_template_ok);

test void test_expand_filename_leading_zeroes_ok(void) {
	// given

	// when
	int chars = expand_filename("gbsplay-%S.%e", "wav", 3);

	// then
	ASSERT_EQUAL("chars written %d", chars, 15);
	ASSERT_STRING_EQUAL("filename %s", filename, "gbsplay-004.wav");
}
TEST(test_expand_filename_leading_zeroes_ok);

test void test_expand_filename_multiple_placeholders_ok(void) {
	// given

	// when
	int chars = expand_filename("%e.%s-%S-%s.foo", "wav", 49);

	// then
	ASSERT_EQUAL("chars written %d", chars, 17);
	ASSERT_STRING_EQUAL("filename %s", filename, "wav.50-050-50.foo");
}
TEST(test_expand_filename_multiple_placeholders_ok);

test void test_expand_filename_unknown_percent_sequence_parsed_literally(void) {
	// given

	// when
	int chars = expand_filename("gbsplay-%?.%e", "wav", 5);

	// then
	ASSERT_EQUAL("chars written %d", chars, 14);
	ASSERT_STRING_EQUAL("filename %s", filename, "gbsplay-%?.wav");
}
TEST(test_expand_filename_unknown_percent_sequence_parsed_literally);

test void test_expand_filename_too_long(void) {
	// given

	// when
	int chars = expand_filename("gbsplay-%s.%e", "superlongextension", 10);

	// then
	ASSERT_EQUAL("chars written %d", chars, 29); // reported size is longer than actual written size!
	ASSERT_STRING_EQUAL("filename %s", filename, "gbsplay-11.superlongex");
}
TEST(test_expand_filename_too_long);

TEST_EOF;

#endif /* ENABLE_TEST */
