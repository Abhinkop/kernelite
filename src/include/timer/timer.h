/**
 * @file timer.h
 * @brief System timer interface.
 *
 * Declares the kernel-facing API for configuring the periodic tick and
 * registering its interrupt handler. Implementations dispatch to the
 * platform-specific timer driver underneath.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-06-28
 */

#ifndef TIMER_TIMER_H
#define TIMER_TIMER_H

#include "icu/icu.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Configure the system timer's tick period and interrupt handler.
 *
 * @param fdt Pointer to the Flattened Device Tree (FDT) blob, which may be
 *            used to discover timer properties and configuration.
 * @param milliseconds Desired tick period in milliseconds.
 * @param handler Handler function and private data to invoke when the
 *                timer interrupt fires.
 * @return true if the timer was configured successfully, false otherwise.
 */
bool setup_sys_timer(const void *fdt, uint64_t milliseconds,
		     handler_data_t handler);

#endif /* TIMER_TIMER_H */
