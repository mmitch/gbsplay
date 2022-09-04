/*
 * gbsplay is a Gameboy sound player
 *
 * write audio data to file
 *
 * 2022 (C) by Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#ifndef _FILEWRITER_H_
#define _FILEWRITER_H_

#include "common.h"

int   file_write_bytes(FILE* file, const void* const start, const size_t count);
int   file_write_string(FILE* file, const char* const string);
long  file_write_32bit_placeholder(FILE* file);

int   file_write_16bit_le(FILE* file, const uint16_t value);
int   file_write_32bit_le(FILE* file, const uint32_t value);
int   file_write_32bit_le_at(FILE* file, const uint32_t data, const long offset);

int   file_write_16bit_be(FILE* file, const uint16_t value);
int   file_write_32bit_be(FILE* file, const uint32_t value);
int   file_write_32bit_be_at(FILE* file, const uint32_t data, const long offset);

FILE* file_open(const char* const extension, const int subsong);
int   file_close(FILE* file);
long  file_get_position(FILE* const file);

#endif
