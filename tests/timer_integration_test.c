/**
 * @file timer_integration_test.c
 * @brief Integration tests for the ARM generic timer driver.
 */

#include "../src/include/asm/asm_helper.h"
#include "../src/include/icu/icu.h"

#include "test.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief CNTFRQ_EL0 must be non-zero.
 *
 * A zero frequency would make every timer period calculation divide-by-zero.
 * It also indicates the register is unreadable, which would mean the timer
 * cannot be used at all.
 */
static bool test_cntfrq_nonzero(void)
{
	uint64_t freq = 0;
	READ_SYS_REG(cntfrq_el0, freq);

	EXPECT(freq > 0);
	return true;
}

/**
 * @brief CNTPCT_EL0 must advance between two reads.
 *
 * Reads the counter twice with a brief busy loop between them.  If the
 * second value is not greater than the first the system counter has halted
 * or is not implemented, which would prevent the timer from ever firing.
 */
static bool test_cntpct_advances(void)
{
	uint64_t before = 0;
	uint64_t after = 0;

	READ_SYS_REG(cntpct_el0, before);

	for (volatile int i = 0; i < 64; i++) {
		asm volatile("nop");
	}

	READ_SYS_REG(cntpct_el0, after);

	EXPECT(after > before);
	return true;
}

/**
 * @brief CNTP_CTL_EL0.ENABLE must be 1 after setup_sys_timer().
 *
 * Bit 0 of CNTP_CTL_EL0 is the enable bit.  The driver sets it at the end
 * of initialisation.  If it is clear the timer will never fire regardless
 * of the comparator value.
 */
static bool test_cntp_ctl_enabled(void)
{
	uint64_t ctl = 0;
	READ_SYS_REG(cntp_ctl_el0, ctl);

	EXPECT(ctl & 0x1); /* ENABLE bit */
	return true;
}

/**
 * @brief CNTP_CTL_EL0.IMASK must be 0 after setup_sys_timer().
 *
 * Bit 1 is the interrupt mask.  The driver must leave it clear so that
 * when the comparator fires the interrupt is actually forwarded to the GIC.
 * A masked timer fires the internal ISTATUS bit but never generates a PPI.
 */
static bool test_cntp_ctl_not_masked(void)
{
	uint64_t ctl = 0;
	READ_SYS_REG(cntp_ctl_el0, ctl);

	EXPECT(!(ctl & 0x2));
	return true;
}

/**
 * @brief CNTP_CVAL_EL0 must be set to a future value.
 *
 * The driver programs an absolute comparator (CVAL = CNTPCT + period).
 * If CVAL equals zero the timer either hasn't been programmed yet or was
 * incorrectly reset.  A zero CVAL would cause an immediate and unrecoverable
 * flood of timer interrupts.
 */
static bool test_cntp_cval_nonzero(void)
{
	uint64_t cval = 0;
	READ_SYS_REG(cntp_cval_el0, cval);

	EXPECT(cval > 0);
	return true;
}

/** @brief Set by the integration timer handler to confirm delivery. */
static volatile bool timer_integration_fired;

/**
 * @brief Integration timer handler.
 *
 * Records that a timer interrupt was received.  Registered transiently for
 * INTID 30 (EL1 physical timer PPI on QEMU virt) so it runs instead of
 * (and before re-registering) the normal tick handler.
 */
static void timer_integration_handler(void *priv)
{
	(void)priv;
	timer_integration_fired = true;

	/* Adding 1 second's worth of ticks (CNTFRQ_EL0) is conservative. */
	uint64_t freq = 0;
	uint64_t cval = 0;
	READ_SYS_REG(cntfrq_el0, freq);
	READ_SYS_REG(cntp_cval_el0, cval);
	cval += freq;
	WRITE_SYS_REG(cntp_cval_el0, cval);
}

/**
 * @brief The EL1 physical timer PPI must be received within one period.
 *
 * Temporarily replaces the normal timer handler with one that sets
 * @c timer_integration_fired, then waits in WFI until it fires.  The
 * existing CVAL is already set by the normal driver, so the interrupt
 * will arrive without any additional programming.
 *
 * This test exercises the complete timer→GIC→redistributor→CPU-interface→
 * vector→ICU dispatch path in live hardware.
 */
static bool test_timer_ppi_fires(void)
{
	timer_integration_fired = false;
	const uint32_t timer_intid = 30;

	icu_unregister_irq(timer_intid);

	handler_data_t handler = {
		.handler = timer_integration_handler,
		.private_data = NULL,
	};

	EXPECT(icu_register_irq(timer_intid, handler));
	asm volatile("wfi");
	icu_unregister_irq(timer_intid);

	EXPECT(timer_integration_fired);
	return true;
}

test_suite_t get_timer_integration_test_suite(void)
{
	test_suite_t suite = {
		.suite_name = "timer_integration",
		.num_tests = 0,
	};

#define ADD(fn)                                       \
	suite.tests[suite.num_tests].test_name = #fn; \
	suite.tests[suite.num_tests].test_fn = fn;    \
	suite.num_tests++

	ADD(test_cntfrq_nonzero);
	ADD(test_cntpct_advances);
	ADD(test_cntp_ctl_enabled);
	ADD(test_cntp_ctl_not_masked);
	ADD(test_cntp_cval_nonzero);
	ADD(test_timer_ppi_fires);

#undef ADD
	return suite;
}
