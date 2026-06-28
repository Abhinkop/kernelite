/**
 * @file timer.c
 * @brief Implementation of the system timer interface.
 *
 * Dispatches tick configuration and interrupt handler registration to the
 * platform-specific timer driver.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-06-28
 */

#include "timer/timer.h"

#include "fdt/fdt.h"
#include "utils/kprintf.h"

#include "../drivers/arm_timer.h"

#include <stdint.h>

/** @brief Maximum number of timer nodes the kernel will search for in the FDT.
 */
#define MAX_TIMERS 10

bool setup_sys_timer(const void *fdt, uint64_t milliseconds,
		     handler_data_t handler)
{
	if (fdt == NULL) {
		kprintf("Timer Error: null FDT pointer\n");
		return false;
	}

	int timer_nodes[MAX_TIMERS];

	int num_timer_nodes = get_nodes_by_compatible(fdt, "arm,armv8-timer",
						      timer_nodes, MAX_TIMERS);

	if (num_timer_nodes < 0) {
		kprintf("Timer Error: failed to search FDT for \"arm,armv8-timer\" nodes\n");
		return false;
	}

	if (num_timer_nodes != 1) {
		kprintf("Timer Error: expected exactly 1 \"arm,armv8-timer\" node, found %d\n",
			num_timer_nodes);
		return false;
	}

	return setup_arm64_sys_timer(fdt, timer_nodes[0], milliseconds,
				     handler);
}
