/**
 * @file arm_timer.h
 * @brief ARM generic system timer driver.
 *
 * Declares the platform-specific implementation backing the kernel's
 * timer interface for the ARM generic timer (CNTP).
 *
 * @author Abhin Parekadan Jose
 * @date 2026-06-28
 */

#ifndef DRIVERS_ARM_TIMER_H
#define DRIVERS_ARM_TIMER_H

#include "icu/icu.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Configure the system timer's tick period and interrupt handler.
 *
 * @param fdt Pointer to the Flattened Device Tree (FDT) blob, which may be
 *            used to discover timer properties and configuration.
 * @param node_offset Offset of the timer node in the FDT.
 * @param milliseconds Desired tick period in milliseconds.
 * @param handler Handler function and private data to invoke when the
 *                timer interrupt fires.
 * @return true if the timer was configured successfully, false otherwise.
 */
bool setup_arm64_sys_timer(const void *fdt, int node_offset,
			   uint64_t milliseconds, handler_data_t handler);

#endif /* DRIVERS_ARM_TIMER_H */
