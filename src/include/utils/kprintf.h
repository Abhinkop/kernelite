/**
 * @file kprintf.h
 * @brief Utility functions for formatted output and serial console management.
 *
 * Provides a freestanding implementation of kprintf for kernel-level debugging
 * and serial console output in a freestanding environment.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-16
 */

#ifndef UTILS_KPRINTF_H
#define UTILS_KPRINTF_H

/**
 * @struct serial_t
 * @brief Hardware abstraction layer for serial I/O operations.
 *
 * This structure maps generic I/O requests to hardware-specific
 * driver functions (e.g., PL011 UART).
 */
typedef struct {
	/** @brief Function pointer to transmit a single character over the
	 * serial port. */
	void (*putc)(char chr);

	/** @brief Function pointer to receive a single character from the
	 * serial port. */
	char (*getc)();
} serial_t;

/**
 * @brief Configures the global serial console used by kprintf.
 *
 * Sets the function pointers that kprintf will use to output characters.
 *
 * @param console A serial_t structure containing initialized function
 * pointers.
 */
void set_kprintf_console(serial_t console);

/**
 * @brief Standard formatted print to the serial console.
 *
 * A wrapper around vprintf that handles variadic argument initialization
 * and cleanup.
 *
 * @param format Formatting string containing plain text and specifiers.
 * @param ...    Variable arguments matching the specifiers in the format
 * string.
 * @return       The total number of characters successfully printed.
 */
int kprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));

#endif // UTILS_KPRINTF_H
