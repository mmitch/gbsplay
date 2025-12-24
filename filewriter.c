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

#define FILENAME_SIZE 23

static char filename[FILENAME_SIZE];

int expand_filename(const char* const filename_template, const char* const extension, const int subsong) {
	char* const last = filename + FILENAME_SIZE - 1;
	const char *src;
	char *dst;

	for (src = filename_template, dst = filename; *src != 0 && dst < last; src++) {
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

	if (dst > last || *src != 0) {
		fprintf(stderr, "%s\n", _("Output filename too long!"));
		*(last) = 0;
		return 1;
	}

	*dst = 0;
	return 0;
}

FILE* file_open(const char* const extension, const int subsong) {
	FILE* file = NULL;

	if (expand_filename(cfg.output_filename, extension, subsong)  != 0)
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

#define CANARY "CANARY"

struct player_cfg cfg;
char* canary_start = filename + FILENAME_SIZE;

#define ASSERT_RC_OK(rc)     ASSERT_EQUAL("rc %d", rc, 0)
#define ASSERT_RC_FAILED(rc) ASSERT_EQUAL("rc %d", rc, 1)
#define ASSERT_CANARY_OK()   ASSERT_STRING_EQUAL("canary", canary_start, CANARY)

test void setup_canary() {
	sprintf(canary_start, CANARY);
}
TEST(setup_canary);

test void test_expand_filename_default_template_ok(void) {
	// given

	// when
	int result = expand_filename("gbsplay-%s.%e", "wav", 0);

	// then
	ASSERT_RC_OK(result);
	ASSERT_STRING_EQUAL("filename", filename, "gbsplay-1.wav");
	ASSERT_CANARY_OK();
}
TEST(test_expand_filename_default_template_ok);

test void test_expand_filename_leading_zeroes_ok(void) {
	// given

	// when
	int result = expand_filename("gbsplay-%S.%e", "wav", 3);

	// then
	ASSERT_RC_OK(result);
	ASSERT_STRING_EQUAL("filename", filename, "gbsplay-004.wav");
	ASSERT_CANARY_OK();
}
TEST(test_expand_filename_leading_zeroes_ok);

test void test_expand_filename_multiple_placeholders_ok(void) {
	// given

	// when
	int result = expand_filename("%e.%s-%S-%s.foo", "wav", 49);

	// then
	ASSERT_RC_OK(result);
	ASSERT_STRING_EQUAL("filename", filename, "wav.50-050-50.foo");
	ASSERT_CANARY_OK();
}
TEST(test_expand_filename_multiple_placeholders_ok);

test void test_expand_filename_unknown_percent_sequence_parsed_literally_ok(void) {
	// given

	// when
	int result = expand_filename("gbsplay-%?.%e", "wav", 5);

	// then
	ASSERT_RC_OK(result);
	ASSERT_STRING_EQUAL("filename", filename, "gbsplay-%?.wav");
	ASSERT_CANARY_OK();
}
TEST(test_expand_filename_unknown_percent_sequence_parsed_literally_ok);

test void test_expand_filename_too_long_after_expansion_fail(void) {
	// given

	// when
	int result = expand_filename("gbsplay-%s.%e", "superlongextension", 10);

	// then
	ASSERT_RC_FAILED(result);
	ASSERT_STRING_EQUAL("filename", filename, "gbsplay-11.superlongex");
	ASSERT_CANARY_OK();
}
TEST(test_expand_filename_too_long_after_expansion_fail);

test void test_expand_filename_template_longer_than_target_fail(void) {
	// given

	// when
	int result = expand_filename("aaaaabbbbbcccccdddddeeeee", "ext", 0);

	// then
	ASSERT_RC_FAILED(result);
	ASSERT_STRING_EQUAL("filename %s", filename, "aaaaabbbbbcccccdddddee"); // cut off after 22 chars, filename[23] == 0
	ASSERT_CANARY_OK();
}
TEST(test_expand_filename_template_longer_than_target_fail);

TEST_EOF;

#endif /* ENABLE_TEST */
