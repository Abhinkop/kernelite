/**
 * @file fdt_test.c
 * @brief Tests for FDT helper functions that do not require a real DTB.
 */

#include "../src/include/fdt/fdt.h"

#include "test.h"

#include <stdint.h>

/**
 * @brief PPI number 14 must map to INTID 30 (PPI base 16 + 14).
 */
static bool test_ppi_intid_timer(void)
{
	interrupt_t intr = {
		.type = IRQ_TYPE_PPI,
		.id = 14,
		.trigger = IRQ_TRIGGER_LEVEL_HIGH,
	};

	int32_t intid = interrupt_to_intid(&intr);

	EXPECT_EQ(intid, 30);
	return true;
}

/**
 * @brief PPI number 0 must map to INTID 16 (the PPI base offset itself).
 */
static bool test_ppi_intid_base(void)
{
	interrupt_t intr = {
		.type = IRQ_TYPE_PPI,
		.id = 0,
		.trigger = IRQ_TRIGGER_LEVEL_HIGH,
	};

	EXPECT_EQ(interrupt_to_intid(&intr), 16);
	return true;
}

/**
 * @brief PPI number 15 must map to INTID 31 (highest standard PPI).
 */
static bool test_ppi_intid_max(void)
{
	interrupt_t intr = {
		.type = IRQ_TYPE_PPI,
		.id = 15,
		.trigger = IRQ_TRIGGER_LEVEL_HIGH,
	};

	EXPECT_EQ(interrupt_to_intid(&intr), 31);
	return true;
}

/**
 * @brief SPI number 0 must map to INTID 32 (the SPI base offset itself).
 */
static bool test_spi_intid_base(void)
{
	interrupt_t intr = {
		.type = IRQ_TYPE_SPI,
		.id = 0,
		.trigger = IRQ_TRIGGER_EDGE,
	};

	EXPECT_EQ(interrupt_to_intid(&intr), 32);
	return true;
}

/**
 * @brief SPI number 64 must map to INTID 96 (SPI base 32 + 64).
 */
static bool test_spi_intid_offset(void)
{
	interrupt_t intr = {
		.type = IRQ_TYPE_SPI,
		.id = 64,
		.trigger = IRQ_TRIGGER_LEVEL_HIGH,
	};

	EXPECT_EQ(interrupt_to_intid(&intr), 96);
	return true;
}

/**
 * @brief An unrecognised interrupt type must return -1 and not crash.
 */
static bool test_unknown_type_returns_minus_one(void)
{
	interrupt_t intr = {
		// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
		.type = (irq_type_t)0xFF, /* invalid */
		.id = 0,
		.trigger = IRQ_TRIGGER_EDGE,
	};

	int32_t intid = interrupt_to_intid(&intr);

	EXPECT_EQ(intid, -1);
	return true;
}

/**
 * @brief Trigger field must not affect the INTID calculation.
 */
static bool test_trigger_does_not_affect_intid(void)
{
	interrupt_t edge = {
		.type = IRQ_TYPE_SPI,
		.id = 5,
		.trigger = IRQ_TRIGGER_EDGE,
	};
	interrupt_t level = {
		.type = IRQ_TYPE_SPI,
		.id = 5,
		.trigger = IRQ_TRIGGER_LEVEL_HIGH,
	};

	EXPECT_EQ(interrupt_to_intid(&edge), interrupt_to_intid(&level));
	return true;
}

test_suite_t get_fdt_test_suite(void)
{
	test_suite_t suite = {
		.suite_name = "fdt",
		.num_tests = 0,
	};

#define ADD(fn)                                       \
	suite.tests[suite.num_tests].test_name = #fn; \
	suite.tests[suite.num_tests].test_fn = fn;    \
	suite.num_tests++

	ADD(test_ppi_intid_timer);
	ADD(test_ppi_intid_base);
	ADD(test_ppi_intid_max);
	ADD(test_spi_intid_base);
	ADD(test_spi_intid_offset);
	ADD(test_unknown_type_returns_minus_one);
	ADD(test_trigger_does_not_affect_intid);

#undef ADD
	return suite;
}
