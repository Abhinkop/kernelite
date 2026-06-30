/**
 * @file string_test.c
 * @brief Tests for the kernel string/memory utility functions.
 */

#include "../src/include/utils/string.h"
#include "test.h"

#include <stddef.h>
#include <stdint.h>

// NOLINTBEGIN(*DeprecatedOrUnsafeBufferHandling)

/** @brief Copies a known pattern and verifies each byte at the destination. */
static bool test_memcpy_basic(void)
{
	const uint8_t src[] = { 0x01, 0x02, 0x03, 0x04 };
	uint8_t dst[4] = { 0 };

	void *ret = memcpy(dst, src, 4);

	EXPECT_EQ(dst[0], 0x01);
	EXPECT_EQ(dst[1], 0x02);
	EXPECT_EQ(dst[2], 0x03);
	EXPECT_EQ(dst[3], 0x04);
	EXPECT_EQ((uintptr_t)ret, (uintptr_t)dst);
	return true;
}

/** @brief Zero-byte copy must not touch the destination. */
static bool test_memcpy_zero_size(void)
{
	uint8_t dst[4] = { 0xAA, 0xAA, 0xAA, 0xAA };
	const uint8_t src[] = { 0x01, 0x02, 0x03, 0x04 };

	memcpy(dst, src, 0);

	EXPECT_EQ(dst[0], 0xAA);
	return true;
}

/** @brief Identical buffers must compare equal. */
static bool test_memcmp_equal(void)
{
	const uint8_t aaa[] = { 1, 2, 3 };
	const uint8_t bbb[] = { 1, 2, 3 };

	EXPECT_EQ(memcmp(aaa, bbb, 3), 0);
	return true;
}

/** @brief First buffer greater than second must return positive. */
static bool test_memcmp_greater(void)
{
	const uint8_t aaa[] = { 1, 3, 3 };
	const uint8_t bbb[] = { 1, 2, 3 };

	EXPECT(memcmp(aaa, bbb, 3) > 0);
	return true;
}

/** @brief First buffer less than second must return negative. */
static bool test_memcmp_less(void)
{
	const uint8_t aaa[] = { 1, 2, 3 };
	const uint8_t bbb[] = { 1, 2, 4 };

	EXPECT(memcmp(aaa, bbb, 3) < 0);
	return true;
}

/** @brief Zero-byte comparison must always return 0. */
static bool test_memcmp_zero_size(void)
{
	const uint8_t aaa[] = { 0xFF };
	const uint8_t bbb[] = { 0x00 };

	EXPECT_EQ(memcmp(aaa, bbb, 0), 0);
	return true;
}

/** @brief Every byte in the target range must be set to the given value. */
static bool test_memset_basic(void)
{
	uint8_t buf[8] = { 0 };

	void *ret = memset(buf, 0xAB, 8);

	for (size_t i = 0; i < 8; i++) {
		EXPECT_EQ(buf[i], 0xAB);
	}
	EXPECT_EQ((uintptr_t)ret, (uintptr_t)buf);
	return true;
}

/** @brief memset with value 0 must zero the buffer. */
static bool test_memset_zero(void)
{
	uint8_t buf[4] = { 0xFF, 0xFF, 0xFF, 0xFF };

	memset(buf, 0, 4);

	for (size_t i = 0; i < 4; i++) {
		EXPECT_EQ(buf[i], 0x00);
	}
	return true;
}

/** @brief Only the low 8 bits of the value are used. */
static bool test_memset_truncates_to_byte(void)
{
	uint8_t buf[4] = { 0 };

	// NOLINTNEXTLINE(bugprone-suspicious-memset-usage)
	memset(buf, 0x1FF, 4);

	EXPECT_EQ(buf[0], 0xFF);
	return true;
}

/** @brief Must return a pointer to the first matching byte. */
static bool test_memchr_found(void)
{
	const uint8_t buf[] = { 1, 2, 3, 4, 3 };

	void *chr_ptr = memchr(buf, 3, 5);

	EXPECT_NOT_NULL(chr_ptr);
	EXPECT_EQ((uintptr_t)chr_ptr, (uintptr_t)&buf[2]);
	return true;
}

/** @brief Must return NULL when the byte is absent. */
static bool test_memchr_not_found(void)
{
	const uint8_t buf[] = { 1, 2, 3 };

	EXPECT_NULL(memchr(buf, 0xFF, 3));
	return true;
}

/** @brief Zero-length search must return NULL even if the byte matches. */
static bool test_memchr_zero_size(void)
{
	const uint8_t buf[] = { 0xAA };

	EXPECT_NULL(memchr(buf, 0xAA, 0));
	return true;
}

/** @brief Non-overlapping memmove must produce the same result as memcpy. */
static bool test_memmove_non_overlapping(void)
{
	const uint8_t src[] = { 10, 20, 30 };
	uint8_t dst[3] = { 0 };

	memmove(dst, src, 3);

	EXPECT_EQ(dst[0], 10);
	EXPECT_EQ(dst[1], 20);
	EXPECT_EQ(dst[2], 30);
	return true;
}

/** @brief Forward overlap: dst > src, bytes must be copied correctly. */
static bool test_memmove_forward_overlap(void)
{
	uint8_t buf[] = { 1, 2, 3, 4, 5 };

	/* move buf[0..2] to buf[1..3] — destination overlaps source */
	memmove(&buf[1], &buf[0], 4);

	EXPECT_EQ(buf[1], 1);
	EXPECT_EQ(buf[2], 2);
	EXPECT_EQ(buf[3], 3);
	EXPECT_EQ(buf[4], 4);
	return true;
}

/** @brief Backward overlap: dst < src, copy must not clobber source. */
static bool test_memmove_backward_overlap(void)
{
	uint8_t buf[] = { 1, 2, 3, 4, 5 };

	/* move buf[1..3] to buf[0..2] */
	memmove(&buf[0], &buf[1], 4);

	EXPECT_EQ(buf[0], 2);
	EXPECT_EQ(buf[1], 3);
	EXPECT_EQ(buf[2], 4);
	EXPECT_EQ(buf[3], 5);
	return true;
}

/** @brief Standard null-terminated string. */
static bool test_strlen_basic(void)
{
	EXPECT_EQ(strlen("hello"), 5);
	return true;
}

/** @brief Empty string must return 0. */
static bool test_strlen_empty(void)
{
	EXPECT_EQ(strlen(""), 0);
	return true;
}

/** @brief Returns string length when string is shorter than maxlen. */
static bool test_strnlen_shorter_than_max(void)
{
	EXPECT_EQ(strnlen("abc", 10), 3);
	return true;
}

/** @brief Returns maxlen when string is longer than maxlen. */
static bool test_strnlen_capped_at_max(void)
{
	EXPECT_EQ(strnlen("abcde", 3), 3);
	return true;
}

/** @brief Zero maxlen must always return 0. */
static bool test_strnlen_zero_max(void)
{
	EXPECT_EQ(strnlen("abc", 0), 0);
	return true;
}

/** @brief Returns pointer to first occurrence. */
static bool test_strchr_found(void)
{
	const char *str = "abcabc";
	const char *chr_ptr = strchr(str, 'b');

	EXPECT_NOT_NULL(chr_ptr);
	EXPECT_EQ((uintptr_t)chr_ptr, (uintptr_t)(str + 1));
	return true;
}

/** @brief strchr must find the null terminator itself. */
static bool test_strchr_finds_null_terminator(void)
{
	const char *str = "abc";
	const char *chr_ptr = strchr(str, '\0');

	EXPECT_NOT_NULL(chr_ptr);
	EXPECT_EQ((uintptr_t)chr_ptr, (uintptr_t)(str + 3));
	return true;
}

/** @brief Returns NULL when the character is absent. */
static bool test_strchr_not_found(void)
{
	EXPECT_NULL(strchr("abc", 'z'));
	return true;
}

/** @brief Returns pointer to the last occurrence. */
static bool test_strrchr_found_last(void)
{
	const char *str = "abcabc";
	const char *chr_ptr = strrchr(str, 'b');

	EXPECT_NOT_NULL(chr_ptr);
	EXPECT_EQ((uintptr_t)chr_ptr, (uintptr_t)(str + 4));
	return true;
}

/** @brief Returns NULL when the character is absent. */
static bool test_strrchr_not_found(void)
{
	EXPECT_NULL(strrchr("abc", 'z'));
	return true;
}

/** @brief Equal strings return 0. */
static bool test_strcmp_equal(void)
{
	EXPECT_EQ(strcmp("hello", "hello"), 0);
	return true;
}

/** @brief First string less than second returns negative. */
static bool test_strcmp_less(void)
{
	EXPECT(strcmp("abc", "abd") < 0);
	return true;
}

/** @brief First string greater than second returns positive. */
static bool test_strcmp_greater(void)
{
	EXPECT(strcmp("abd", "abc") > 0);
	return true;
}

/** @brief Prefix is less than the full string. */
static bool test_strcmp_prefix_is_less(void)
{
	EXPECT(strcmp("abc", "abcd") < 0);
	return true;
}

/** @brief Empty strings compare equal. */
static bool test_strcmp_both_empty(void)
{
	EXPECT_EQ(strcmp("", ""), 0);
	return true;
}

/** @brief Parse a decimal number. */
static bool test_strtoul_decimal(void)
{
	EXPECT_EQ(strtoul("12345", NULL, 10), 12345UL);
	return true;
}

/** @brief Parse a hexadecimal number with the 0x prefix (base 0). */
static bool test_strtoul_hex_autodetect(void)
{
	EXPECT_EQ(strtoul("0xFF", NULL, 0), 255UL);
	return true;
}

/** @brief Parse a hexadecimal number without the prefix (base 16). */
static bool test_strtoul_hex_explicit_base(void)
{
	EXPECT_EQ(strtoul("FF", NULL, 16), 255UL);
	return true;
}

/** @brief endptr must point to the first non-digit character. */
static bool test_strtoul_endptr(void)
{
	const char *str = "42abc";
	char *end = NULL;

	unsigned long val = strtoul(str, &end, 10);

	EXPECT_EQ(val, 42UL);
	EXPECT_NOT_NULL(end);
	EXPECT_EQ((uintptr_t)end, (uintptr_t)(str + 2));
	return true;
}

/** @brief Zero must parse correctly. */
static bool test_strtoul_zero(void)
{
	EXPECT_EQ(strtoul("0", NULL, 10), 0UL);
	return true;
}

/** @brief Non-numeric string at start gives 0 and leaves endptr at start. */
static bool test_strtoul_no_digits(void)
{
	const char *str = "xyz";
	char *end = NULL;

	unsigned long val = strtoul(str, &end, 10);

	EXPECT_EQ(val, 0UL);
	EXPECT_EQ((uintptr_t)end, (uintptr_t)str);
	return true;
}

test_suite_t get_string_test_suite(void)
{
	test_suite_t suite = {
		.suite_name = "string",
		.num_tests = 0,
	};

#define ADD(fn)                                       \
	suite.tests[suite.num_tests].test_name = #fn; \
	suite.tests[suite.num_tests].test_fn = fn;    \
	suite.num_tests++

	ADD(test_memcpy_basic);
	ADD(test_memcpy_zero_size);
	ADD(test_memcmp_equal);
	ADD(test_memcmp_greater);
	ADD(test_memcmp_less);
	ADD(test_memcmp_zero_size);
	ADD(test_memset_basic);
	ADD(test_memset_zero);
	ADD(test_memset_truncates_to_byte);
	ADD(test_memchr_found);
	ADD(test_memchr_not_found);
	ADD(test_memchr_zero_size);
	ADD(test_memmove_non_overlapping);
	ADD(test_memmove_forward_overlap);
	ADD(test_memmove_backward_overlap);
	ADD(test_strlen_basic);
	ADD(test_strlen_empty);
	ADD(test_strnlen_shorter_than_max);
	ADD(test_strnlen_capped_at_max);
	ADD(test_strnlen_zero_max);
	ADD(test_strchr_found);
	ADD(test_strchr_finds_null_terminator);
	ADD(test_strchr_not_found);
	ADD(test_strrchr_found_last);
	ADD(test_strrchr_not_found);
	ADD(test_strcmp_equal);
	ADD(test_strcmp_less);
	ADD(test_strcmp_greater);
	ADD(test_strcmp_prefix_is_less);
	ADD(test_strcmp_both_empty);
	ADD(test_strtoul_decimal);
	ADD(test_strtoul_hex_autodetect);
	ADD(test_strtoul_hex_explicit_base);
	ADD(test_strtoul_endptr);
	ADD(test_strtoul_zero);
	ADD(test_strtoul_no_digits);

#undef ADD
	return suite;
}

// NOLINTEND(*DeprecatedOrUnsafeBufferHandling)
