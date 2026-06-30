/**
 * @file mem_layout_test.c
 * @brief Tests for VA/PA address translation helpers.
 */

#include "../src/include/mem_layout/mem_layout.h"
#include "test.h"

#include <stdint.h>

/**
 * @brief va_to_pa is identity when kernel_base_va is zero.
 *
 * Before the high-half switch, every virtual address equals its physical
 * address.  Breaking this assumption would corrupt all pre-switch memory
 * accesses.
 */
static bool test_va_to_pa_identity_pre_switch(void)
{
	virt_addr v_addr = 0x40080000UL;

	EXPECT_EQ(va_to_pa(v_addr), (phy_addr)0x40080000UL);
	return true;
}

/**
 * @brief pa_to_va is identity when kernel_base_va is zero.
 */
static bool test_pa_to_va_identity_pre_switch(void)
{
	phy_addr p_addr = 0x40080000UL;

	EXPECT_EQ(pa_to_va(p_addr), (virt_addr)0x40080000UL);
	return true;
}

/**
 * @brief Zero address translates to zero in both directions.
 */
static bool test_zero_address_pre_switch(void)
{
	EXPECT_EQ(va_to_pa(0), (phy_addr)0);
	EXPECT_EQ(pa_to_va(0), (virt_addr)0);
	return true;
}

test_suite_t get_mem_layout_test_suite(void)
{
	test_suite_t suite = {
		.suite_name = "mem_layout",
		.num_tests = 0,
	};

#define ADD(fn)                                       \
	suite.tests[suite.num_tests].test_name = #fn; \
	suite.tests[suite.num_tests].test_fn = fn;    \
	suite.num_tests++

	/*
	 * Only pre-switch tests here. Tests run before the MMU is enabled
	 * so kernel_base_va == 0 and va_to_pa/pa_to_va are identity functions.
	 *
	 * The post-switch tests (pa_to_va_post_switch, round_trip_post_switch)
	 * call update_kernel_base_va() which permanently sets kernel_base_va
	 * to 0xFFFF000000000000. That would break all subsequent
	 * setup_page_allocator() calls in the runner (which internally use
	 * va_to_pa). Those tests belong in a future integration suite that
	 * runs after high_half_main() is entered.
	 */
	ADD(test_va_to_pa_identity_pre_switch);
	ADD(test_pa_to_va_identity_pre_switch);
	ADD(test_zero_address_pre_switch);

#undef ADD
	return suite;
}
