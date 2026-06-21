/**
 * @file uart.h
 * @brief PL011 UART hardware abstraction layer.
 *
 * Declares the device struct and initialisation function for the ARM PL011
 * UART used as the kernel's early serial console.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-04-25
 */

#ifndef DRIVERS_UART_H
#define DRIVERS_UART_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Hardware abstraction for a UART device.
 */
typedef struct uart_device {
	/** @brief Base address for Memory-Mapped I/O */
	uintptr_t mmio_base;

	/** @brief Function pointer to transmit a character */
	void (*putc)(struct uart_device *dev, char c);

	/** @brief Function pointer to receive a character */
	char (*getc)(struct uart_device *dev);
} uart_device_t;

/**
 * @brief Initialize a UART device struct.
 * @param dev Pointer to the device struct to initialize.
 * @param base MMIO base address.
 */
void pl011_init(uart_device_t *dev, uintptr_t base);

/**
 * @brief Register the PL011 UART receive interrupt handler with the ICU.
 *
 * Installs pl011_rx_handler into the ICU dispatch table for INTID 33
 * (the UART SPI on QEMU virt) and enables the interrupt in the GIC.
 * After this call, any character received on the UART will trigger
 * pl011_rx_handler which echoes it back.
 *
 * @param dev Pointer to the PL011 UART device struct to register.
 *            Passed as private_data to the handler on each invocation.
 * @return true if the handler was registered successfully, false if
 *         INTID 33 is already registered or GIC configuration failed.
 */
bool pl011_register_handler(uart_device_t *dev);

#endif /* DRIVERS_UART_H */
