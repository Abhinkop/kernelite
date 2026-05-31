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

#include "drivers/uart.h"
#include "page_table/page_table.h"
#include "utils/kprintf.h"
#include "fdt/fdt.h"
#include "utils/utils.h"
#include "linker/symbols.h"
#include "mmu/mmu.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <libfdt.h>

#ifdef RUN_TESTS

/**
 * @brief Run internal kernel tests.
 * * This function is called when the RUN_TESTS flag is set during compilation.
 * * It executes a suite of internal tests to validate kernel functionality
 * before proceeding with normal operation. The results of the tests are
 * printed to the console, and the system exits with an appropriate code based
 * on the test outcomes.
 * @param fdt_addr Pointer to the Device Tree Blob (FDT) address passed by the
 * bootloader.
 */
extern void run_internal_tests(const void *fdt_addr);

#endif /* RUN_TESTS */

static uart_device_t uart0;

/**
 * @brief Captured bootloader arguments.
 * * Stores the raw values of registers x0 through x3 as passed by the
 * bootloader (e.g., U-Boot) at the moment of kernel entry.
 * * - boot_args[0]: Physical address of the Device Tree Blob (FDT).
 * - boot_args[1]: Reserved (0).
 * - boot_args[2]: Reserved (0).
 * - boot_args[3]: Reserved (0).
 */
uint64_t boot_args[4];

/**
 * @brief UART0 Character Output Function.
 * * This function is designed to be used as the 'putc' function in the
 * serial_t structure for kprintf. It abstracts the hardware-specific details
 * of writing a character to the UART0 data register.
 * * @param chr The character to be transmitted over UART0.
 */
void uart0_putchar(char chr)
{
	uart0.putc(&uart0, chr);
}

/**
 * @brief Print the memory map.
 * @param mmap Pointer to the memory map structure.
 */
void print_memory_map(const Memory_map_t *mmap)
{
	kprintf("Memory Map (Total Regions: %d):\n", mmap->count);
	for (int i = 0; i < mmap->count; i++) {
		kprintf("  Region %d: Base = 0x%lx, Size = 0x%lx bytes\n", i,
			mmap->regions[i].base, mmap->regions[i].size);
	}
}

/**
 * @brief Kernel Main Entry Point.
 * * Called from primary_entry (boot.s) after the stack has been initialized
 * and the BSS section has been cleared.
 * @note This function should never return.
 * @param boot_args_ptr Pointer to an array containing the bootloader arguments
 * passed in registers x0-x3.
 * @return exit code
 */
int main(const uint64_t *boot_args_ptr)
{
	virt_addr uart0_base = 0x09000000;
	pl011_init(&uart0, uart0_base);
	set_kprintf_console((serial_t){ .putc = uart0_putchar, .getc = NULL });

	// NOLINTNEXTLINE(*-int-to-ptr)
	const void *fdt_addr = (const void *)boot_args_ptr[0];

#ifdef RUN_TESTS
	run_internal_tests(fdt_addr);
#endif

	// This needs to done befor setting up the global allocator.
	// As it sets a page allocator for the static `idmap_pg_dir_start` area
	if (!setup_kernel_id_map()) {
		kprintf("Error while setting up id map\n");
		return 1;
	}

	// Id map uart before setting up allocator.
	bool uart_mapped =
		map_page(get_id_map_root(), uart0_base, uart0_base,
			 (page_permissions_t){ .execute = false,
					       .read = true,
					       .write = true,
					       .user_accessible = false },
			 device);
	if (!uart_mapped) {
		kprintf("Error while id mapping uart\n");
		return 1;
	}

	dump_memory_map(get_id_map_root());

	kprintf("fdt adrr = 0x%lx size = 0x%lx\n", (virt_addr)fdt_addr,
		fdt_totalsize(fdt_addr));

	if (!enable_mmu((page_table_t *)get_id_map_root())) {
		kprintf("Error while setting up mmu\n");
		return 1;
	}

	// setup_global_allocator(fdt_addr);
	// Do this after setting up the kernal high maapings.

	kprintf("Hello World!\n");

	return 0; // returns calls exit with code 0.
}
