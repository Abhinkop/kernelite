/**
 * @file gic_integration_test.c
 * @brief Integration tests for the GICv3 driver.
 */

#include "../src/include/asm/asm_helper.h"
#include "../src/include/icu/icu.h"
#include "../src/include/utils/utils.h"
#include "../src/kernel/drivers/gicv3.h"

#include "test.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief ICC_SRE_EL1.SRE must be 1 after gicv3_init().
 */
static bool test_icc_sre_enabled(void)
{
	uint64_t sre = 0;
	READ_SYS_REG(icc_sre_el1, sre);

	EXPECT(sre & 0x1);
	return true;
}

/**
 * @brief ICC_PMR_EL1 must be 0xFF or the implementation-masked equivalent.
 */
static bool test_icc_pmr_all_unmasked(void)
{
	uint64_t pmr = 0;
	READ_SYS_REG(icc_pmr_el1, pmr);

	/* At least the top 4 priority bits must be set (0xF0 minimum). */
	EXPECT((pmr & 0xFFUL) >= (uint32_t)0xF0);
	return true;
}

/**
 * @brief ICC_IGRPEN1_EL1.enable must be 1 after gicv3_init().
 */
static bool test_icc_igrpen1_enabled(void)
{
	uint64_t igrpen = 0;
	READ_SYS_REG(icc_igrpen1_el1, igrpen);

	EXPECT(igrpen & 0x1);
	return true;
}

/**
 * @brief gicv3_read_gicd_ctlr() must return a non-zero raw value after init.
 *
 * A zero CTLR would mean ARE_NS and EnableGrp1NS are both clear, which
 * means no interrupts are being forwarded.  Checks that the read-back
 * accessor actually reaches hardware (non-zero = register is mapped).
 */
static bool test_gicd_ctlr_readable(void)
{
	uint32_t ctlr = gicv3_read_gicd_ctlr();

	EXPECT(ctlr != 0);
	return true;
}

/**
 * @brief GICD_CTLR must indicate affinity routing is enabled.
 *
 * On a single-security-state GIC (DS == 1, the QEMU virt default), the
 * operative affinity-routing enable is ARE_S (bit 4); ARE_NS (bit 5) is
 * RES0.  On a dual-security-state GIC both ARE_S and ARE_NS are distinct.
 * Accepting either means the test is correct for both configurations.
 */
static bool test_gicd_ctlr_are_ns_set(void)
{
	uint32_t ctlr = gicv3_read_gicd_ctlr();

	/* ARE is enabled if ARE_NS (dual-security) or ARE_S (DS==1) is set. */
	EXPECT(ctlr & (0x3U << 4));
	return true;
}

/**
 * @brief GICD_CTLR.EnableGrp1NS must be 1.
 *
 * Without this, Group 1 NS interrupts are not forwarded by the distributor.
 */
static bool test_gicd_ctlr_grp1ns_enabled(void)
{
	uint32_t ctlr = gicv3_read_gicd_ctlr();

	EXPECT(ctlr & (0x1U << 1));
	return true;
}

/**
 * @brief GICD_CTLR.RWP must be 0 (no write pending).
 *
 * The driver polls RWP after each CTLR write.  If RWP is still set after
 * init, a subsequent GICD register write during live operation could race
 * with the pending configuration update and produce undefined behaviour.
 */
static bool test_gicd_ctlr_rwp_clear(void)
{
	uint32_t ctlr = gicv3_read_gicd_ctlr();

	EXPECT(!(ctlr & (0x1U << 3)));
	return true;
}

/**
 * @brief GICD_TYPER.ITLinesNumber must be at least 1 (>= 64 SPIs).
 *
 * QEMU virt exposes enough SPIs for the timer PPI and user drivers.  A
 * value of 0 would mean the GIC has no SPIs at all, which contradicts the
 * device tree.  This also sanity-checks that the GICD base address mapped
 * correctly (a wrong address would read all-zeros or all-ones).
 */
static bool test_gicd_typer_itlines_nonzero(void)
{
	uint32_t typer = gicv3_read_gicd_typer();

	EXPECT((typer & 0x1FU) > 0);
	return true;
}

/**
 * @brief GICR_WAKER.ChildrenAsleep must be 0 after the driver wakes it.
 *
 * The driver clears GICR_WAKER.ProcessorSleep and polls until
 * ChildrenAsleep drops.  If ChildrenAsleep is still set, SGI and PPI
 * interrupts will not be forwarded by the redistributor.
 */
static bool test_gicr_waker_children_awake(void)
{
	volatile uint32_t *waker = gicv3_get_waker();
	EXPECT_NOT_NULL(waker);

	uint32_t res = *waker;

	EXPECT((res & (1U << 2)) == 0);
	return true;
}

/**
 * @brief GICR_WAKER.ProcessorSleep must be 0 after the driver wakes it.
 */
static bool test_gicr_waker_processor_awake(void)
{
	volatile uint32_t *waker = gicv3_get_waker();
	EXPECT_NOT_NULL(waker);

	uint32_t res = *waker;

	EXPECT((res & (1U << 1)) == 0);
	return true;
}

/** @brief Set by the integration SGI handler to confirm delivery. */
static volatile bool sgi_integration_fired;

/**
 * @brief Handler for the integration SGI round-trip test.
 */
static void sgi_integration_handler(void *priv)
{
	(void)priv;
	sgi_integration_fired = true;
}

/**
 * @brief A self-triggered SGI must be received and handled end-to-end.
 *
 * This is the most comprehensive single test: it exercises the GICv3
 * distributor, redistributor, CPU interface, vector table, and ICU
 * dispatch path in one shot.  Any break in the interrupt pipeline —
 * wrong INTID routing, wrong group, EOI mode mismatch, VBAR misconfigured
 * — will cause the flag to remain false and the test to fail.
 *
 * Uses INTID 0 (SGI 0) which is distinct from the INTID 11 used by main().
 */
static bool test_sgi_round_trip(void)
{
	sgi_integration_fired = false;

	handler_data_t handler = {
		.handler = sgi_integration_handler,
		.private_data = NULL,
	};

	EXPECT(icu_register_irq(0, handler));

	gicv3_trigger_sgi(0, get_core_id());

	asm volatile("wfi");

	icu_unregister_irq(0);

	EXPECT(sgi_integration_fired);
	return true;
}

test_suite_t get_gic_integration_test_suite(void)
{
	test_suite_t suite = {
		.suite_name = "gic_integration",
		.num_tests = 0,
	};

#define ADD(fn)                                       \
	suite.tests[suite.num_tests].test_name = #fn; \
	suite.tests[suite.num_tests].test_fn = fn;    \
	suite.num_tests++

	ADD(test_icc_sre_enabled);
	ADD(test_icc_pmr_all_unmasked);
	ADD(test_icc_igrpen1_enabled);
	ADD(test_gicd_ctlr_readable);
	ADD(test_gicd_ctlr_are_ns_set);
	ADD(test_gicd_ctlr_grp1ns_enabled);
	ADD(test_gicd_ctlr_rwp_clear);
	ADD(test_gicd_typer_itlines_nonzero);
	ADD(test_gicr_waker_children_awake);
	ADD(test_gicr_waker_processor_awake);
	ADD(test_sgi_round_trip);

#undef ADD
	return suite;
}
