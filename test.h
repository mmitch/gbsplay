/*
 * gbsplay is a Gameboy sound player
 *
 * 2011 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 * Licensed under GNU GPL.
 */

#ifndef _TEST_H_
#define _TEST_H_

#include <stdio.h>

#define ASSERT_EQUAL(fmt, a, b) do { \
	typeof(a) aa; \
	typeof(a) bb; \
	aa = a; \
	bb = b; \
	if (aa != bb) { \
		fprintf(stderr, "FAIL\nTest failed: "fmt"!="fmt" at %s:%d\n", aa, bb, __FILE__, __LINE__); \
		exit(1); \
	} \
} while(0)

#ifdef ENABLE_TEST

#if defined(__GNUC__) && !defined(__APPLE__)

#include "config.h"
#include "common.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define test __attribute__((section(".test")))
#define test_entries __attribute__((section(".test_entries")))

typedef void (*test_fn)(void);
struct test_entry {
	test_fn func;
	const char* name;
};

#define TEST(func) test_entries struct test_entry test_ ## func = { func, #func }

#define TEST_EOF test_entries struct test_entry test__end = { 0 };
test_entries struct test_entry test__head = { 0 };
test_entries struct test_entry test__align = { 0 };
extern struct test_entry test__end;

int main(int argc, char** argv)
{
	void *head = &test__head;
	void *align = &test__align;
	if ((align - head) != sizeof(test__head)) {
		fprintf(stderr, "Expected alignment constraints don't hold!\n");
		exit(1);
	}
	struct test_entry *tests = &test__head;
	int num_tests = (&test__end - &test__head);
	int i;
	printf(" %d tests:\n", num_tests-2);
	for (i=2; i<num_tests; i++) {
		printf("    %s: ", tests[i].name);
		tests[i].func();
		printf("ok\n");
	}
	return 0;
}

#else

int main(int argc, char** argv)
{
	printf(" builtin tests not supported on this platform.\n");
	return 0;
}

#endif /* defined(__GNUC__) && !defined(__APPLE__) */

#endif /* ENABLE_TEST */

#ifndef TEST_EOF

#define test static __attribute__((unused))
#define TEST(func) static __attribute__((unused)) int test_ ## func
#define TEST_EOF static __attribute__((unused)) int test_eof

#endif

#endif /* _TEST_H_ */
