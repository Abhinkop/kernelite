/**
 * @file arm_timer.c
 * @brief Implementation of the ARM generic system timer driver (CNTP).
 *
 * @author Abhin Parekadan Jose
 * @date 2026-06-28
 */

#include "arm_timer.h"

#include "utils/kprintf.h"
#include "fdt/fdt.h"
#include "asm/asm_helper.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Maximum number of "interrupts" entries the timer node may have. */
#define MAX_TIMER_INTERRUPTS 4

/** @brief Index of the non-secure EL1 physical timer in the ARM generic
 *  timer's standard 4-entry "interrupts" property (secure, non-secure,
 *  virtual, hypervisor). This is the PPI fired by CNTP_*. */
#define NS_PHYSICAL_TIMER_INTR_INDEX 1

/** @brief Number of milliseconds in a second, used to derive CNTP_TVAL_EL0
 *  ticks from a millisecond period and CNTFRQ_EL0. */
#define MILLISECONDS_PER_SECOND 1000U

/** @brief Number of ticks CNTP_TVAL_EL0 must be loaded with to fire once
 *  per configured tick period; set by set_period() and consumed by
 *  timer_isr() to re-arm the timer after each interrupt. */
static uint64_t period_ticks;

/** @brief Handler registered by the caller of setup_arm64_sys_timer(),
 *  invoked from timer_isr() on every tick. */
static handler_data_t tick_handler;

/**
 * @brief CNTP_CTL_EL0 (EL1 Physical Timer Control Register) layout.
 */
typedef union {
	/** @brief Raw 64-bit register value, for use with READ/WRITE_SYS_REG.
	 */
	uint64_t raw;

	struct {
		/** @brief ENABLE: Enables the timer. */
		bool is_enable : 1;

		/** @brief IMASK: Masks the timer interrupt when set. */
		bool is_masked : 1;

		/** @brief ISTATUS: Set if the timer condition is met
		 *  (read-only; writes are ignored). */
		bool is_condition_fulfilled : 1;
	};
} cntp_ctl_el0_t;

/**
 * @brief ISR for the EL1 physical timer interrupt.
 *
 * Masks the timer, invokes the registered tick handler if the timer
 * condition was actually met, reloads CNTP_TVAL_EL0 for the next period,
 * and unmasks/re-enables the timer.
 *
 * @param priv Unused; registered with NULL private data via icu_register_irq().
 */
static void timer_isr(void *priv)
{
	(void)priv;

	cntp_ctl_el0_t reg = { 0 };
	READ_SYS_REG(CNTP_CTL_EL0, reg);

	if (!reg.is_condition_fulfilled) {
		return;
	}

	reg.is_masked = true;
	WRITE_SYS_REG(CNTP_CTL_EL0, reg);

	tick_handler.handler(tick_handler.private_data);

	uint64_t cval;
	READ_SYS_REG(CNTP_CVAL_EL0, cval);
	cval += period_ticks;
	WRITE_SYS_REG(CNTP_CVAL_EL0, cval);

	reg.is_enable = true;
	reg.is_masked = false;
	WRITE_SYS_REG(CNTP_CTL_EL0, reg);
}

/**
 * @brief Compute and store the CNTP_TVAL_EL0 tick count for a tick period.
 *
 * @param period_ms Desired tick period in milliseconds.
 * @return true if the period fits in CNTP_TVAL_EL0, false otherwise.
 */
static bool set_period(uint64_t period_ms)
{
	uint64_t freq = 0;
	READ_SYS_REG(CNTFRQ_EL0, freq);
	kprintf("CNTFRQ_EL0 = %lu\n", freq);

	period_ticks = (freq * period_ms) / MILLISECONDS_PER_SECOND;

	if (period_ticks > UINT32_MAX) {
		kprintf("ARM Timer Error: period %lu ms does not fit in CNTP_TVAL_EL0\n",
			period_ms);
		return false;
	}
	return true;
}

/**
 * @brief Register the tick handler and enable the EL1 physical timer.
 *
 * @param int_id GIC INTID of the EL1 physical timer interrupt.
 * @param handler Handler function and private data to invoke on each tick.
 * @return true if the ISR was registered and the timer enabled successfully,
 * false otherwise.
 */
static bool setup_isr(uint32_t int_id, handler_data_t handler)
{
	tick_handler.handler = handler.handler;
	tick_handler.private_data = handler.private_data;

	handler_data_t private_handler = {
		.handler = timer_isr,
		.private_data = NULL,
	};
	if (!icu_register_irq(int_id, private_handler)) {
		kprintf("ARM Timer Error: failed to register IRQ %u\n", int_id);
		return false;
	}

	uint64_t now;
	READ_SYS_REG(CNTPCT_EL0, now);
	WRITE_SYS_REG(CNTP_CVAL_EL0, now + period_ticks);

	cntp_ctl_el0_t reg = { 0 };
	reg.is_enable = true;
	reg.is_masked = false;

	WRITE_SYS_REG(CNTP_CTL_EL0, reg);
	return true;
}

bool setup_arm64_sys_timer(const void *fdt, int node_offset,
			   uint64_t milliseconds, handler_data_t handler)
{
	if (fdt == NULL) {
		kprintf("ARM Timer Error: null FDT pointer\n");
		return false;
	}

	if (fdt_is_error(node_offset)) {
		kprintf("ARM Timer Error: invalid timer node offset %d\n",
			node_offset);
		return false;
	}

	interrupt_t intrs[MAX_TIMER_INTERRUPTS];

	int num_intrs = get_intr_property(fdt, node_offset, intrs,
					  MAX_TIMER_INTERRUPTS);

	if (num_intrs < 0) {
		kprintf("ARM Timer Error: failed to read \"interrupts\" property (error %d)\n",
			num_intrs);
		return false;
	}

	if (num_intrs <= NS_PHYSICAL_TIMER_INTR_INDEX) {
		kprintf("ARM Timer Error: expected at least %d \"interrupts\" entries (non-secure PPI is index %d), found %d\n",
			NS_PHYSICAL_TIMER_INTR_INDEX + 1,
			NS_PHYSICAL_TIMER_INTR_INDEX, num_intrs);
		return false;
	}

	if (!set_period(milliseconds)) {
		return false;
	}

	int32_t intid =
		interrupt_to_intid(&intrs[NS_PHYSICAL_TIMER_INTR_INDEX]);
	if (intid < 0) {
		kprintf("ARM Timer Error: failed to resolve INTID for non-secure physical timer interrupt\n");
		return false;
	}

	return setup_isr((uint32_t)intid, handler);
}
