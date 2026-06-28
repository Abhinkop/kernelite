/**
 * @file kprintf.c
 * @brief Implementation of formatted print functions for kernel debugging.
 *
 * Implements variadic print helpers and serial output wiring for the in-kernel
 * diagnostic console.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-16
 */

#include "utils/kprintf.h"

#include <stddef.h>
#include <stdarg.h>

/** @brief Global serial console. */
serial_t serial_console;

/**
 * @brief Convert an unsigned integer to a string using the given base.
 *
 * This helper writes the string representation of @p num into @p buf. The
 * resulting string is always null-terminated.
 *
 * @param num  Number to convert.
 * @param base Numerical base for conversion (e.g. 10 for decimal, 16 for hex).
 * @param buf  Buffer to receive the converted string. Must be large enough to
 *             hold the result.
 */
static void itoa(unsigned long num, int base, char *buf)
{
	char *ptr = buf;
	char *ptr1 = buf;
	char *ptr2 = NULL;
	unsigned long digit;

	// Build string in reverse
	do {
		digit = num % base;
		// NOLINTNEXTLINE(bugprone-narrowing-conversions)
		*ptr++ = (digit < 10UL) ? (digit + '0') : (digit - 10 + 'a');
	} while ((num /= base) > 0);

	*ptr = '\0';

	// Reverse the string in place
	ptr1 = buf;
	ptr2 = ptr - 1;
	while (ptr1 < ptr2) {
		char tmp = *ptr1;
		*ptr1++ = *ptr2;
		*ptr2-- = tmp;
	}
}

/**
 * @brief Variadic print function.
 *
 * Processes the format string and arguments, outputting characters via the
 * configured serial console.
 *
 * Supports: %%c, %%s, %%d, %%u, %%x, %%p. A leading 'l' length modifier
 * (e.g. %lx) is accepted, but the character after 'l' is consumed and
 * ignored — any %l followed by any character is always printed as
 * unsigned hex, regardless of what that character actually is.
 *
 * @param format Formatting string containing plain text and specifiers.
 * @param args   An initialized va_list containing the arguments to be
 * formatted.
 * @return       The total number of characters successfully printed.
 */
// NOLINTNEXTLINE(*-cognitive-complexity)
int vprintf(const char *format, va_list args)
{
	if (serial_console.putc == NULL)
		return 0; // No console set, can't print

	char buf[64];
	const char *ptr = NULL;
	int printed = 0;

	for (ptr = format; *ptr != '\0'; ptr++) {
		if (*ptr != '%') {
			if (*ptr == '\n') {
				serial_console.putc('\r');
				printed++;
			}
			serial_console.putc(*ptr);
			printed++;
			continue;
		}

		ptr++; // Skip '%'
		if (*ptr == 'l') {
			ptr++;
		}

		switch (*ptr) {
		case 'x':
		case 'p': {
			if (*ptr == 'p') {
				serial_console.putc('0');
				serial_console.putc('x');
				printed += 2;
			}
			unsigned long num = va_arg(args, unsigned long);

			itoa(num, 16, buf);
			for (char *str = buf; *str; str++) {
				serial_console.putc(*str);
				printed++;
			}
			break;
		}
		case 'c': {
			char chr = (char)va_arg(args, int);

			serial_console.putc(chr);
			printed++;
			break;
		}
		case 's': {
			char *str = va_arg(args, char *);

			if (!str)
				str = "(null)";
			while (*str) {
				serial_console.putc(*str++);
				printed++;
			}
			break;
		}
		case 'd': {
			long num = va_arg(args, long);

			if (num < 0) {
				serial_console.putc('-');
				printed++;
				num = -num;
			}
			itoa((unsigned long)num, 10, buf);
			for (char *str = buf; *str; str++) {
				serial_console.putc(*str);
				printed++;
			}
			break;
		}
		case 'u': {
			unsigned long num = va_arg(args, unsigned long);

			itoa(num, 10, buf);
			for (char *str = buf; *str; str++) {
				serial_console.putc(*str);
				printed++;
			}
			break;
		}
		case '%': {
			serial_console.putc('%');
			printed++;
			break;
		}
		default: {
			serial_console.putc('%');
			serial_console.putc(*ptr);
			printed += 2;
			break;
		}
		}
	}
	return printed;
}

void set_kprintf_console(serial_t con)
{
	serial_console.getc = con.getc;
	serial_console.putc = con.putc;
}

int kprintf(const char *format, ...)
{
	int printed = 0;
	va_list args;

	va_start(args, format);
	// NOLINTNEXTLINE(clang-analyzer-valist*)
	printed = vprintf(format, args);
	va_end(args);

	return printed;
}
