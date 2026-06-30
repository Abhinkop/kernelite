/**
 * @file test_runner.c
 * @brief Test runner for executing kernel tests under QEMU.
 *
 * Tests run in main() BEFORE the MMU is enabled, so execution is at low
 * physical addresses.  Function pointers to suite getters stored in a data
 * table would hold high link-time VAs that are unreachable without TTBR1.
 * Suite getters must therefore be called directly (PC-relative bl) and their
 * returned suite_t processed immediately on the stack, one at a time.
 *
 * BSS clobbering: page_init() zeroes the bitmap region which is adjacent to
 * BSS in the linked image.  Any static (BSS) variable in test code will be
 * wiped on the second setup_page_allocator() call.  All mutable test state
 * must live on the stack.
 */

#include "../src/include/utils/kprintf.h"
#include "../src/include/utils/utils.h"

extern void exit(uint32_t code);

#include "test.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ── Suite getter declarations ───────────────────────────────────────── */

extern test_suite_t get_linker_symbol_test_suite(void);
extern test_suite_t get_page_table_test_suite(void);
extern test_suite_t get_string_test_suite(void);
extern test_suite_t get_mem_layout_test_suite(void);
extern test_suite_t get_fdt_test_suite(void);
extern test_suite_t get_page_allocator_test_suite(void);

/**
 * @brief Exit QEMU with a specific code.
 * @param code 0 = all tests passed, 1 = at least one test failed.
 */
static inline void qemu_exit(uint32_t code)
{
	volatile uint64_t block[2];
	block[0] = 0x20026;
	block[1] = code;

	// NOLINTBEGIN(hicpp-no-assembler)
	__asm__ volatile("mov x0, #0x18\n" /* SYS_EXIT */
			 "mov x1, %0\n" /* pointer to block */
			 "hlt #0xF000\n" ::"r"((uint64_t)block)
			 : "x0", "x1", "memory");
	// NOLINTEND(hicpp-no-assembler)
}

/**
 * @brief Run all tests in one suite and update the running totals.
 *
 * The suite is passed by pointer to avoid copying ~1 KB on every call.
 * setup_page_allocator() is called before each individual test to reset
 * the allocator bitmap to a known-clean state.
 *
 * @param suite     Pointer to the populated suite to execute.
 * @param fdt_addr  FDT blob address forwarded to setup_page_allocator().
 * @param passed    [in/out] Accumulated pass count.
 * @param failed    [in/out] Accumulated fail count.
 */
static void run_suite(const test_suite_t *suite, const void *fdt_addr,
		      size_t *passed, size_t *failed)
{
	kprintf("\n[suite] %s (%lu tests)\n", suite->suite_name,
		suite->num_tests);

	size_t suite_passed = 0;
	size_t suite_failed = 0;

	for (size_t j = 0; j < suite->num_tests; j++) {
		if (!setup_page_allocator(fdt_addr)) {
			kprintf("  FATAL: setup_page_allocator failed before"
				" '%s' -- aborting\n",
				suite->tests[j].test_name);
			qemu_exit(1);
		}

		// NOLINTNEXTLINE(readability-identifier-length)
		bool ok = suite->tests[j].test_fn();

		if (ok) {
			kprintf("  [ PASS ] %s\n", suite->tests[j].test_name);
			suite_passed++;
		} else {
			kprintf("  [ FAIL ] %s\n", suite->tests[j].test_name);
			suite_failed++;
		}
	}

	kprintf("  --> %lu passed, %lu failed\n", suite_passed, suite_failed);
	*passed += suite_passed;
	*failed += suite_failed;
}

/**
 * @brief Run all registered kernel test suites.
 *
 * Called from main() before the MMU is enabled.  Each suite is obtained,
 * run, and discarded in sequence.  The returned suite_t lives on the stack
 * only for the duration of the run_suite() call.
 *
 * To add a new suite: declare its getter extern above, then add a
 * RUN_SUITE() call below.
 *
 * @param fdt_addr  Pointer to the Device Tree Blob passed by the bootloader.
 */
void run_internal_tests(const void *fdt_addr)
{
	kprintf("\n");
	kprintf("========================================\n");
	kprintf("  kernelite test runner\n");
	kprintf("========================================\n");

	size_t total_passed = 0;
	size_t total_failed = 0;

	/*
	 * Each RUN_SUITE call:
	 *   1. Calls the getter via a direct PC-relative bl (no function
	 *      pointer dereference, so no high-VA problem).
	 *   2. Stores the returned suite in a local variable (stack, ~1 KB).
	 *   3. Passes it to run_suite() and discards it.
	 *
	 * The local `test_suite` variable is in a nested block so the compiler
	 * can reuse its stack slot for the next suite.
	 */
#define RUN_SUITE(getter)                                       \
	do {                                                    \
		test_suite_t test_suite = getter();             \
		run_suite(&test_suite, fdt_addr, &total_passed, \
			  &total_failed);                       \
	} while (0)

	RUN_SUITE(get_linker_symbol_test_suite);
	RUN_SUITE(get_page_table_test_suite);
	RUN_SUITE(get_string_test_suite);
	RUN_SUITE(get_mem_layout_test_suite);
	RUN_SUITE(get_fdt_test_suite);
	RUN_SUITE(get_page_allocator_test_suite);

#undef RUN_SUITE

	kprintf("\n========================================\n");
	kprintf("  TOTAL: %lu passed, %lu failed\n", total_passed,
		total_failed);
	kprintf("========================================\n\n");

	if (total_failed > 0) {
		kprintf("Unit tests FAILED -- aborting before integration tests\n");
		qemu_exit(1);
	}
}

/* ── Integration test runner (post-MMU, post-GIC-init) ──────────────── */

extern test_suite_t get_gic_integration_test_suite(void);

/**
 * @brief Run gic & timer tests from high_half_main() after all drivers init.
 */
void run_gic_and_timer_tests(void)
{
	kprintf("\n");
	kprintf("========================================\n");
	kprintf("  kernelite gic & timer test runner\n");
	kprintf("========================================\n");

	size_t total_passed = 0;
	size_t total_failed = 0;

#define RUN_SUITE(getter)                                                    \
	do {                                                                 \
		test_suite_t test_suite = getter();                          \
		kprintf("\n[suite] %s (%lu tests)\n", test_suite.suite_name, \
			test_suite.num_tests);                               \
		size_t passed = 0;                                           \
		size_t failed = 0;                                           \
		for (size_t _j = 0; _j < test_suite.num_tests; _j++) {       \
			bool _ok = test_suite.tests[_j].test_fn();           \
			if (_ok) {                                           \
				kprintf("  [ PASS ] %s\n",                   \
					test_suite.tests[_j].test_name);     \
				passed++;                                    \
			} else {                                             \
				kprintf("  [ FAIL ] %s\n",                   \
					test_suite.tests[_j].test_name);     \
				failed++;                                    \
			}                                                    \
		}                                                            \
		kprintf("  --> %lu passed, %lu failed\n", passed, failed);   \
		total_passed += passed;                                      \
		total_failed += failed;                                      \
	} while (0)

	RUN_SUITE(get_gic_integration_test_suite);

#undef RUN_SUITE

	kprintf("\n========================================\n");
	kprintf("  GIC & TIMER TEST TOTAL: %lu passed, %lu failed\n",
		total_passed, total_failed);
	kprintf("========================================\n\n");

	exit(total_failed > 0 ? 1 : 0);
}
