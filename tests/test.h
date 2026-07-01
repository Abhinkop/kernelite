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
#include <stdint.h>

/** @brief Maximum number of tests per suite. */
#define MAX_TESTS_PER_SUITE 64

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

/**
 * @brief Fail the test if @p cond is false, printing the condition.
 */
#define EXPECT(cond)                                                       \
	if (!(cond)) {                                                     \
		kprintf("EXPECT failed [%s:%d]: %s\n", __FILE__, __LINE__, \
			#cond);                                            \
		return false;                                              \
	}

/**
 * @brief Fail the test if @p a != @p b
 */
#define EXPECT_EQ(a, b) EXPECT((a) == (b))

/**
 * @brief Fail the test if @p a == @p b.
 */
#define EXPECT_NEQ(a, b) EXPECT((a) != (b))

/**
 * @brief Fail the test if @p a < @p b.
 */
#define EXPECT_GE(a, b) EXPECT((a) >= (b))

/**
 * @brief Fail the test if @p a > @p b.
 */
#define EXPECT_LE(a, b) EXPECT((a) <= (b))

/**
 * @brief Fail the test if @p ptr is not NULL.
 */
#define EXPECT_NULL(ptr)                                                \
	if ((ptr) != NULL) {                                            \
		kprintf("EXPECT_NULL failed [%s:%d]: %s is not NULL\n", \
			__FILE__, __LINE__, #ptr);                      \
		return false;                                           \
	}

/**
 * @brief Fail the test if @p ptr is NULL.
 */
#define EXPECT_NOT_NULL(ptr)                                            \
	if ((ptr) == NULL) {                                            \
		kprintf("EXPECT_NOT_NULL failed [%s:%d]: %s is NULL\n", \
			__FILE__, __LINE__, #ptr);                      \
		return false;                                           \
	}

#endif /* TEST_TEST_H */
