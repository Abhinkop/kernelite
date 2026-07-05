/**
 * @file string.h
 * @brief Minimal string and memory manipulation functions for bare-metal
 * AArch64.
 *
 * Provides essential string and memory helpers used by the kernel and linked
 * external libraries in a freestanding runtime environment.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-16
 */

#ifndef UTILS_STRING_H
#define UTILS_STRING_H

#include <stddef.h>

// NOLINTBEGIN(*-redundant-declaration, *-parameter-name)

/**
 * @brief Copies @p size bytes from memory area @p src to memory area @p dest.
 * @note The memory areas must not overlap. Use memmove() if they do.
 *
 * @param dest Pointer to the destination array.
 * @param src  Pointer to the source of data to be copied.
 * @param size    Number of bytes to copy.
 * @return     A pointer to the destination @p dest.
 */
void *memcpy(void *dest, const void *src, size_t size);

/**
 * @brief Compares the first @p size bytes of memory areas @p str1 and @p str2.
 *
 * @param str1 Pointer to the first memory block.
 * @param str2 Pointer to the second memory block.
 * @param size  Number of bytes to compare.
 * @return   An integer less than, equal to, or greater than zero if @p str1 is
 * found, respectively, to be less than, to match, or be greater than @p str2.
 */
int memcmp(const void *str1, const void *str2, size_t size);

/**
 * @brief Scans memory for a specific character.
 *
 * Searches the first @p size bytes of the memory area pointed to by @p str
 * for the first occurrence of @p chr.
 *
 * @param str Pointer to the memory area.
 * @param chr The character to search for (interpreted as an unsigned char).
 * @param size The number of bytes to analyze.
 * @return  A pointer to the matching byte, or NULL if the character is not
 * found.
 */
void *memchr(const void *str, int chr, size_t size);

/**
 * @brief Copies @p size bytes from memory area @p src to memory area @p dest.
 * @note  The memory areas may overlap: copying takes place as though the bytes
 * in @p src are first copied into a temporary array that does not overlap
 * @p src or @p dest.
 *
 * @param dest Pointer to the destination array.
 * @param src  Pointer to the source of data to be copied.
 * @param size    Number of bytes to copy.
 * @return     A pointer to the destination @p dest.
 */
void *memmove(void *dest, const void *src, size_t size);

/**
 * @brief Fills the first @p size bytes of the memory area pointed to by @p
 * s_ptr with the constant byte @p val.
 *
 * @param s_ptr Pointer to the memory area to fill.
 * @param val The byte value to set.
 * @param size Number of bytes to be set to the value.
 * @return  A pointer to the memory area @p s_ptr.
 */
void *memset(void *s_ptr, int val, size_t size);

/**
 * @brief Bounds-checked fill of @p dest with a constant byte.
 *
 * Writes @p count copies of @p byte into @p dest after validating that the
 * write fits within @p destsz. The operation is rejected if @p dest is NULL,
 * @p destsz is zero, or @p count exceeds @p destsz.
 *
 * @param dest   Pointer to the memory area to fill.
 * @param destsz Size of the destination buffer, in bytes.
 * @param byte   The byte value to write.
 * @param count  Number of bytes to set.
 * @return 0 on success, or -1 if the arguments fail validation.
 */
int memset_s(void *dest, size_t destsz, int byte, size_t count);

/**
 * @brief Locate the first occurrence of a character in a string.
 * @param str The string to search.
 * @param chr The character to look for (passed as int, converted to char).
 * @return A pointer to the matched character, or NULL if not found.
 */
char *strchr(const char *str, int chr);

/**
 * @brief Locates the last occurrence of a character in a string.
 *
 * Scans the string @p str to find the last occurrence of character @p chr.
 *
 * @param str Pointer to the null-terminated string.
 * @param chr The character to search for.
 * @return  A pointer to the last occurrence of the character, or NULL if not
 * found.
 */
char *strrchr(const char *str, int chr);

/**
 * @brief Calculates the length of a string.
 *
 * Scans the string @p str until the terminating null byte ('\0') is found.
 *
 * @param str Pointer to the null-terminated string.
 * @return  The number of characters in the string, excluding the null byte.
 */
size_t strlen(const char *str);

/**
 * @brief Calculates the length of a string up to a specified maximum.
 *
 * Similar to strlen(), but scans at most @p maxlen bytes.
 *
 * @param str      Pointer to the string.
 * @param maxlen The maximum number of bytes to examine.
 * @return       The number of characters in the string if less than @p maxlen;
 * otherwise, @p maxlen.
 */
size_t strnlen(const char *str, size_t maxlen);

/**
 * @brief Convert a string to an unsigned long integer.
 * @param nptr The string containing the representation of the number.
 * @param endptr Pointer to the character that stopped the scan.
 * @param base The radix to use (e.g., 10 for decimal, 16 for hex).
 * @return The converted value.
 */
// NOLINTNEXTLINE(readability-redundant-declaration)
unsigned long strtoul(const char *nptr, char **endptr, int base);

/**
 * @brief Compare two null-terminated strings.
 * @param lhs The first string to compare.
 * @param rhs The second string to compare.
 * @return
 * - 0 if the strings are equal.
 * - A negative value if @p lhs is less than @p rhs.
 * - A positive value if @p lhs is greater than @p rhs.
 */
int strcmp(const char *lhs, const char *rhs);

// NOLINTEND(*-redundant-declaration, *-parameter-name)

#endif // UTILS_STRING_H
