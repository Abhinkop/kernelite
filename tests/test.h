/**
 * @file test.h
 * @brief Test definitions for kernel testing framework.
 * This header defines the structures and function prototypes used for
 * implementing kernel tests. It includes definitions for test functions, test
 * cases, and test suites.
 */

#ifndef TEST_TEST_H
#define TEST_TEST_H

#include "../src/include/utils/kprintf.h"

#include <stdbool.h>
#include <stddef.h>

/** @brief Maximum number of tests per suite. */
#define MAX_TESTS_PER_SUITE 10

/** @brief Type definition for a test function pointer. */
typedef bool (*test_fn_t)(void);

/** @brief Structure representing a single test case. */
typedef struct test {
	const char *test_name;
	test_fn_t test_fn;
} test_t;

/** @brief Type definition for a test suite. */
typedef struct test_suite {
	const char *suite_name;
	test_t tests[MAX_TESTS_PER_SUITE];
	size_t num_tests;
} test_suite_t;

/** @brief Macro for asserting test conditions. */
#define EXPECT(cond)                                 \
	if (!(cond)) {                               \
		kprintf("Test failed: %s\n", #cond); \
		return false;                        \
	}

#endif /* TEST_TEST_H */
