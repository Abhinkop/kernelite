/**
 * @file main.c
 * @brief Main kernel entry point and primitive UART driver.
 *
 * Handles early kernel initialization, UART setup, FDT parsing, page
 * allocator bootstrapping, and the transition into the kernel runtime loop.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-04-25
 */

#include "allocator/page_allocator.h"
#include "asm/asm_helper.h"
#include "drivers/uart.h"
#include "fdt/fdt.h"
#include "icu/icu.h"
#include "linker/symbols.h"
#include "mem_layout/mem_layout.h"
#include "mmu/mmu.h"
#include "page_table/page_table.h"
#include "timer/timer.h"
#include "utils/kprintf.h"
#include "utils/utils.h"

#include <libfdt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef RUN_TESTS

/**
 * @brief Run internal kernel tests.
 *
 * This function is called when the RUN_TESTS flag is set during compilation.
 * It executes a suite of internal tests to validate kernel functionality
 * before proceeding with normal operation. The results of the tests are
 * printed to the console, and the system exits with an appropriate code based
 * on the test outcomes.
 * @param fdt_addr Pointer to the Device Tree Blob (FDT) address passed by the
 * bootloader.
 */
extern void run_internal_tests(const void *fdt_addr);

/**
 * @brief Run integration tests after GIC and timer initialisation.
 */
extern void run_gic_and_timer_tests(void);

#endif /* RUN_TESTS */

/** @brief Switch to high virtual addresses. */
extern void switch_to_high_va(void);

/** Global UART0 device instance */
static uart_device_t uart0;

/**
 * @brief Captured bootloader arguments.
 *
 * boot.s only ever saves x0 into boot_args[0]; boot_args[1..3] are never
 * written and simply read as 0 because the array lives in zeroed BSS.
 *
 * - boot_args[0]: Physical address of the Device Tree Blob (FDT).
 * - boot_args[1]: Unused (reads as 0 due to BSS zero-init).
 * - boot_args[2]: Unused (reads as 0 due to BSS zero-init).
 * - boot_args[3]: Unused (reads as 0 due to BSS zero-init).
 */
uint64_t boot_args[4];

/**
 * @brief UART0 Character Output Function.
 *
 * This function is designed to be used as the 'putc' function in the
 * serial_t structure for kprintf. It abstracts the hardware-specific details
 * of writing a character to the UART0 data register.
 *
 * @param chr The character to be transmitted over UART0.
 */
static void uart0_putchar(char chr)
{
	uart0.putc(&uart0, chr);
}

/**
 * @brief Test handler invoked on every system timer tick.
 *
 * Registered as the tick handler via setup_sys_timer() to verify that the
 * timer driver correctly fires once per configured period.
 *
 * @param priv Unused; registered with NULL private data.
 */
static void timer_tick_handler(void *priv)
{
	(void)priv;

	kprintf("Timer tick handler called\n");

	uint64_t count = 0;
	READ_SYS_REG(CNTPCT_EL0, count);
	kprintf("CNTPCT_EL0 = %lu\n", count);
}

/**
 * @brief Configure the system timer to tick once per second.
 * @param fdt Pointer to the Flattened Device Tree (FDT) blob, which may be
 *            used to discover timer properties and configuration.
 */
static void setup_system_timer(const void *fdt)
{
	const uint64_t period_ms = 1000;

	handler_data_t handler = {
		.handler = timer_tick_handler,
		.private_data = NULL,
	};

	if (!setup_sys_timer(fdt, period_ms, handler)) {
		kprintf("Error setting up system timer\n");
	}
}

/**
 * @brief High-half kernel entry, executed AFTER the switch to high VAs.
 *
 * Must be a separate, non-inlined function so that all PC-relative address
 * materialisation (e.g. &uart0_putchar) happens while the PC is already in the
 * high VA range. It must never return into the low-VA boot/exit path once
 * TTBR0 is disabled.
 *
 * Never returns: after initialization it parks the core in an infinite
 * wfi loop. The trailing exit(0) is unreachable and exists only to satisfy
 * the compiler/static analysis that every path terminates.
 *
 * @param fdt_addr Physical address of the Device Tree Blob, passed through
 * from main().
 */
static void __attribute__((noinline)) high_half_main(phy_addr fdt_addr)
{
	update_kernel_base_va();
	fixup_page_allocator();

	// Re-init the UART and console with high VAs (pointers formed at high
	// PC).
	pl011_init(&uart0, pa_to_va(0x09000000));
	set_kprintf_console((serial_t){ .putc = uart0_putchar, .getc = NULL });

	// Re-init the vector base to the high address before we drop the low
	// map.
	extern char vector_table;
	uintptr_t high_vbar = (phy_addr)&vector_table; // No need to do pa_to_va
						       // here since the vector
						       // table address is
						       // calculated by using
						       // the pc.
	WRITE_SYS_REG(vbar_el1, high_vbar);

	// Now it is safe to disable TTBR0 (the low identity map) hopefully!.
	tcr_reg_t tcr;
	READ_SYS_REG(tcr_el1, tcr.value);
	tcr.epd0 = 1; // Set EPD0 to disable TTBR0
	WRITE_SYS_REG(tcr_el1, tcr.value);
	WRITE_SYS_REG(ttbr0_el1, 0UL); // Clear TTBR0
	asm volatile("tlbi vmalle1is\n\t"
		     "dsb sy\n\t"
		     "isb\n\t"
		     :
		     :
		     : "memory");

	kprintf("Freeing id map page table pages...\n");
	virt_addr id_map_root = (virt_addr)get_id_map_root();
	page_free(va_to_pa(id_map_root), ID_MAP_NUM_PAGES);

	kprintf("Hello World from high-half!\n");

	virt_addr current_pc = 0;
	asm volatile("adr %0, ." : "=r"(current_pc));
	kprintf("Current PC: 0x%lx\n", current_pc);

	// NOLINTNEXTLINE(*-int-to-ptr)
	const void *fdt = (const void *)pa_to_va(fdt_addr);

	icu_init(fdt);

	pl011_register_handler(&uart0);

	setup_system_timer(fdt);

#ifdef RUN_TESTS
	run_gic_and_timer_tests();
#endif

	uint32_t ticks = 0;
	const uint32_t max_num_of_ticks = 5;

	while (ticks < max_num_of_ticks) {
		asm volatile("wfi");
		ticks++;
	}

	exit(0);
}

/**
 * @brief Kernel Main Entry Point.
 *
 * Called from primary_entry (boot.s) after the stack has been initialized
 * and the BSS section has been cleared.
 * @note On success this function never returns: it hands off to
 * high_half_main()
 * @param boot_args_ptr Pointer to an array containing the bootloader arguments
 * passed in registers x0-x3.
 * @return 1 on initialization failure (id map setup, UART mapping, or FDT
 * memory map parsing failed). Does not return on success.
 */
int main(const uint64_t *boot_args_ptr)
{
	virt_addr uart0_base = 0x09000000;
	pl011_init(&uart0, uart0_base);
	set_kprintf_console((serial_t){ .putc = uart0_putchar, .getc = NULL });

	print_kernelite_logo();

	// NOLINTNEXTLINE(*-int-to-ptr)
	const void *fdt_addr = (const void *)boot_args_ptr[0];

#ifdef RUN_TESTS
	run_internal_tests(fdt_addr);
#endif

	// This needs to be done before setting up the global allocator. It
	// registers a temporary allocator region over the static id-map area
	// and identity-maps the serial console for early output.
	if (!setup_kernel_id_map(uart0_base)) {
		kprintf("Error while setting up id map\n");
		return 1;
	}

	kprintf("fdt adrr = 0x%lx size = 0x%x\n", (virt_addr)fdt_addr,
		fdt_totalsize(fdt_addr));

	memory_map_t mmap;
	if (get_mem(fdt_addr, &mmap) < 0) {
		kprintf("Failed to parse memory map from FDT. Halting.\n");
		return 1;
	}

	// Setup the main page allocator here after setting up the kernel
	// identity map. As the page allocator needs to reserve pages for the
	// kernel image and the FDT itself, which requires the identity map to
	// be functional. also no new pages would be allocated before this point
	// outside of the kernel image, so the static idmap page pool would be
	// sufficient for the initial mappings.
	if (!setup_page_allocator(fdt_addr)) {
		kprintf("Failed to set up page allocator. Halting.\n");
		return 1;
	}

	if (!setup_kernel_map(&mmap)) {
		kprintf("Error while setting up kernel map\n");
		return 1;
	}

	// Map uart to high virtual address.
	bool uart_mapped = map_page(
		get_kernel_map_root(), uart0_base + KERNEL_BASE, uart0_base,
		(page_permissions_t){ .execute = false,
				      .read = true,
				      .write = true,
				      .user_accessible = false },
		DEVICE);
	if (!uart_mapped) {
		kprintf("Error while mapping uart\n");
		return 1;
	}

	if (!enable_mmu(get_id_map_root(), get_kernel_map_root())) {
		kprintf("Error while setting up mmu\n");
		return 1;
	}
	switch_to_high_va();

	// Running at high VAs now. Hand off to a fresh function so all
	// PC-relative addresses are formed at high PC, then never come back.
	high_half_main((phy_addr)fdt_addr);

	__builtin_unreachable();
}
