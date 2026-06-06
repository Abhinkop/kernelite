/**
 * @file uart.c
 * @brief PL011 UART driver implementation.
 *
 * Implements polling-based putc/getc for the ARM PL011 UART and wires them
 * into the uart_device_t hardware abstraction struct.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-04-25
 */

#include "uart.h"
#include <stdint.h>

/** PL011 Data Register offset */
#define UART_DR 0x00

/** PL011 Flag Register offset */
#define UART_FR 0x18

/** Transmit FIFO full */
#define UART_FR_TXFF (1 << 5)

/** Receive FIFO empty */
#define UART_FR_RXFE (1 << 4)

/**
 * @brief PL011 Specific putc implementation.
 * Waits for space in the transmit FIFO, then writes the character to the data
 * register.
 *
 * @param dev Pointer to the UART device struct (mmio_base must be set).
 * @param chr Character to transmit.
 */
static void pl011_putc(uart_device_t *dev, char chr)
{
	// NOLINTNEXTLINE(*-int-to-ptr)
	volatile uint32_t *data_reg = (uint32_t *)(dev->mmio_base + UART_DR);

	*data_reg = (uint32_t)chr;
}

/**
 * @brief PL011 Specific getc implementation.
 * @param dev Pointer to the UART device struct (mmio_base must be set).
 * @return Character received.
 */
static char pl011_getc(uart_device_t *dev)
{
	// NOLINTNEXTLINE(*-int-to-ptr)
	volatile uint32_t *data_reg = (uint32_t *)(dev->mmio_base + UART_DR);
	// NOLINTNEXTLINE(*-int-to-ptr)
	volatile uint32_t *f_reg = (uint32_t *)(dev->mmio_base + UART_FR);

	// Wait for data to arrive
	while (*f_reg & UART_FR_RXFE) {
		__asm__ volatile("nop");
	}

	return (char)(*data_reg & 0xFF);
}

void pl011_init(uart_device_t *dev, uintptr_t base)
{
	dev->mmio_base = base;
	dev->putc = pl011_putc;
	dev->getc = pl011_getc;
}
