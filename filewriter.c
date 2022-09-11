/*
 * gbsplay is a Gameboy sound player
 *
 * write audio data to file
 *
 * 2006-2022 (C) by Christian Garbs <mitch@cgarbs.de>
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

#define FILENAME_SIZE 23

static char filename[FILENAME_SIZE];

int file_write_bytes(FILE* file, const void* const start, const size_t count) {
	return fwrite(start, 1, count, file) != count;
}

int file_write_string(FILE* file, const char* const string) {
	const unsigned long string_length = strlen(string);
	return file_write_bytes(file, string, string_length);
}

int file_write_16bit_le(FILE* file, const uint16_t value) {
	uint8_t le[2];

	le[0] = value;
	le[1] = value >> 8;

	return file_write_bytes(file, le, 2);
}

int file_write_16bit_be(FILE* file, const uint16_t value) {
	uint8_t be[2];

	be[0] = value >> 8;
	be[1] = value;

	return file_write_bytes(file, be, 2);
}

int file_write_32bit_le(FILE* file, const uint32_t value) {
	uint8_t le[4];

	le[0] = value;
	le[1] = value >> 8;
	le[2] = value >> 16;
	le[3] = value >> 24;

	return file_write_bytes(file, le, 4);
}

int file_write_32bit_be(FILE* file, const uint32_t value) {
	uint8_t be[4];

	be[0] = value >> 24;
	be[1] = value >> 16;
	be[2] = value >> 8;
	be[3] = value;

	return file_write_bytes(file, be, 4);
}

int file_write_32bit_le_at(FILE* file, const uint32_t data, const long offset) {
	if (fseek(file, offset, SEEK_SET) == -1)
		return -1;
	return file_write_32bit_le(file, data);
}

int file_write_32bit_be_at(FILE* file, const uint32_t data, const long offset) {
	if (fseek(file, offset, SEEK_SET) == -1)
		return -1;
	return file_write_32bit_be(file, data);
}

long file_write_32bit_placeholder(FILE* file) {
	long offset = ftell(file);
	if (offset == -1)
		return -1;

	if (file_write_32bit_le(file, 0))
		return -1;

	return offset;
}

FILE* file_open(const char* const extension, const int subsong) {
	FILE* file = NULL;

	if (snprintf(filename, FILENAME_SIZE, "gbsplay-%d.%s", subsong + 1, extension) >= FILENAME_SIZE)
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

int file_close(FILE* file) {
	int result = fclose(file);
	file = NULL;
	return result;
}

int file_is_open(FILE* const file) {
	return file != NULL;
}

long file_get_position(FILE* const file) {
	return ftell(file);
}
