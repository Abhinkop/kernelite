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

#include "icu/icu.h"

#include <stdint.h>

/** PL011 Data Register offset */
#define UART_DR 0x00

/** PL011 Flag Register offset */
#define UART_FR 0x18

/** Transmit FIFO full */
#define UART_FR_TXFF (1 << 5)

/** Receive FIFO empty */
#define UART_FR_RXFE (1 << 4)

/** PL011 Interrupt Mask Set/Clear Register offset */
#define UART_IMSC 0x038

/** PL011 Interrupt Clear Register offset */
#define UART_ICR 0x044

/** Receive interrupt mask bit */
#define UART_IMSC_RXIM (1U << 4)

/** Receive interrupt clear bit */
#define UART_ICR_RXIC (1U << 4)

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

/**
 * @brief Enable the PL011 UART receive interrupt.
 *
 * Sets the RXIM bit in UARTIMSC, causing the UART to assert its interrupt
 * line when a character is received. The GIC must also be configured to
 * forward INTID 33 (the UART SPI on QEMU virt) for the interrupt to reach
 * the CPU.
 *
 * @param dev Pointer to the UART device struct (mmio_base must be set).
 */
static void pl011_enable_rx_interrupt(uart_device_t *dev)
{
	volatile uint32_t *imsc =
		// NOLINTNEXTLINE(*-int-to-ptr)
		(volatile uint32_t *)(dev->mmio_base + UART_IMSC);
	*imsc |= UART_IMSC_RXIM;
}

/**
 * @brief Clear the PL011 UART receive interrupt.
 *
 * Writes to UARTICR to acknowledge and clear the receive interrupt.
 * Must be called from the IRQ handler after reading the received character
 * from UARTDR, otherwise the interrupt will immediately re-fire.
 *
 * @param dev Pointer to the UART device struct (mmio_base must be set).
 */
static void pl011_clear_rx_interrupt(uart_device_t *dev)
{
	volatile uint32_t *icr =
		// NOLINTNEXTLINE(*-int-to-ptr)
		(volatile uint32_t *)(dev->mmio_base + UART_ICR);
	*icr = UART_ICR_RXIC;
}

/**
 * @brief PL011 UART receive interrupt handler.
 *
 * Invoked by the ICU dispatch table when INTID 33 (UART SPI on QEMU virt)
 * fires. Reads one character from the UART receive FIFO and echoes it back
 * by writing it to the transmit FIFO.
 *
 * @param data Opaque pointer cast to uart_device_t, identifying which UART
 *             instance raised the interrupt.
 */
static void pl011_rx_handler(void *data)
{
	uart_device_t *dev = (uart_device_t *)data;
	char chr = pl011_getc(dev);
	pl011_putc(dev, chr);
	pl011_clear_rx_interrupt(dev);
}

bool pl011_register_handler(uart_device_t *dev)
{
	handler_data_t temp = {
		.handler = pl011_rx_handler,
		.private_data = dev,
	};
	return icu_register_irq(33, temp);
}

void pl011_init(uart_device_t *dev, uintptr_t base)
{
	dev->mmio_base = base;
	dev->putc = pl011_putc;
	dev->getc = pl011_getc;
	pl011_enable_rx_interrupt(dev);
}
