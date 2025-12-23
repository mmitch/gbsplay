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
#include "test.h"

#define FILENAME_SIZE 23

static char filename[FILENAME_SIZE];

int expand_filename(const char* const extension, const int subsong) {
	return snprintf(filename, FILENAME_SIZE, "gbsplay-%d.%s", subsong + 1, extension);
}

FILE* file_open(const char* const extension, const int subsong) {
	FILE* file = NULL;

	if (expand_filename(extension, subsong) >= FILENAME_SIZE)
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

test void test_expand_filename_ok(void) {
	// given

	// when
	int chars = expand_filename("wav", 0);

	// then
	ASSERT_EQUAL("chars written %d", chars, 13);
	ASSERT_EQUAL("%c", filename[13], 0);
	ASSERT_STRING_EQUAL("filename %s", filename, "gbsplay-1.wav");
}
TEST(test_expand_filename_ok);

test void test_expand_filename_too_long(void) {
	// given

	// when
	int chars = expand_filename("superlongextension", 10);

	// then
	ASSERT_EQUAL("chars written %d", chars, 29); // reported size is longer than actual written size!
	ASSERT_EQUAL("%c", filename[FILENAME_SIZE], 0);
	ASSERT_STRING_EQUAL("filename %s", filename, "gbsplay-11.superlongex");
}
TEST(test_expand_filename_too_long);

TEST_EOF;

#endif /* ENABLE_TEST */
