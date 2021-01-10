#define UNUSED(x) (void)(x)

#define ASSERT(condition) do { \
	if (! (condition) ) { \
		fprintf(stderr, "FAIL\nTest failed:\n[%s]\nat %s:%d\n", #condition, __FILE__, __LINE__); \
		exit(1); \
	} \
} while (0)

#define ASSERT_EQUAL_LONG(actual, expected) do { \
	if ((actual) != (expected) ) { \
		fprintf(stderr, "FAIL\nTest failed:\n[%s] actual <%d> != <%d> expected [%s]\nat %s:%d\n", #actual, actual, expected, #expected, __FILE__, __LINE__); \
		exit(1); \
	} \
} while (0)

#define ASSERT_EQUAL_STRING(actual, expected) do { \
	if (strcmp(actual, expected) !=0 ) { \
		fprintf(stderr, "FAIL\nTest failed:\n[%s] actual <%s> != <%s> expected [%s]\nat %s:%d\n", #actual, actual, expected, #expected, __FILE__, __LINE__); \
		exit(1); \
	} \
} while (0)

