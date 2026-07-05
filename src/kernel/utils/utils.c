/**
 * @file utils.c
 * @brief Implementation of utility functions for kernel development.
 *
 * This module contains utility functions for kernel development, including
 * functions for reserving kernel image pages, setting up the global page
 * allocator, and other helper functions used across the kernel.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-25
 */

#include "utils/utils.h"
#include "allocator/page_allocator.h"
#include "asm/asm_helper.h"
#include "fdt/fdt.h"
#include "linker/symbols.h"
#include "utils/kprintf.h"

#include <libfdt.h>

/**
 * @brief AArch64 CurrentEL register.
 *
 * Contains the current Exception Level. Read-only.
 * The EL field is at bits [3:2]; bits [1:0] are RES0.
 */
typedef union current_el_t {
	/** @brief Raw 64-bit register value. */
	uint64_t raw;
	struct __attribute__((packed)) {
		/** @brief [1:0] Reserved. */
		uint32_t res0 : 2;

		/** @brief [3:2] EL: Current Exception Level.
		 *  0b00 = EL0, 0b01 = EL1, 0b10 = EL2, 0b11 = EL3. */
		uint32_t el : 2;

		/** @brief [63:4] Reserved. */
		uint64_t res0_rest : 60;
	};
} current_el_t;
_Static_assert(sizeof(current_el_t) == 8, "CurrentEL must be 64 bits");

/**
 * @brief Reads the current Exception Level via the CurrentEL system register.
 * @return current_el_t The decoded CurrentEL register value.
 */
static inline current_el_t read_current_el(void)
{
	current_el_t val;
	READ_SYS_REG(CurrentEL, val.raw);
	return val;
}

/**
 * @brief Reserve pages occupied by the kernel image.
 *
 * This function calculates the number of pages occupied by the kernel
 * image based on the linker-provided symbols and reserves those pages in
 * the page allocator to prevent them from being allocated for other purposes.
 *
 * @return bool True if reservation was successful, false otherwise.
 */
static bool reserve_kernel_img_pages(void)
{
	size_t img_size = get_image_size();
	size_t num_pages = NUM_PAGES(img_size);
	void *img_start = (void *)&image_start;

	kprintf("Reserving kernel image pages: start=%p, size=0x%lx bytes, pages=0x%lx\n",
		img_start, img_size, num_pages);

	if (!reserve_page(va_to_pa((virt_addr)img_start), num_pages)) {
		kprintf("Failed to reserve kernel image pages. Halting.\n");
		return false;
	}
	return true;
}

/**
 * @brief Reserve pages occupied by the Device Tree Blob (FDT).
 *
 * This function calculates the number of pages occupied by the FDT
 * and reserves those pages in the page allocator to prevent them from being
 * allocated for other purposes.
 *
 * @param fdt_addr Pointer to the FDT blob.
 * @return bool True if reservation was successful, false otherwise.
 */
static bool reserve_fdt_pages(const void *fdt_addr)
{
	size_t fdt_size = fdt_totalsize(fdt_addr);
	size_t num_pages = NUM_PAGES(fdt_size);

	kprintf("Reserving FDT pages: start=%p, size=0x%lx bytes, pages=0x%lx\n",
		fdt_addr, fdt_size, num_pages);

	if (!reserve_page(va_to_pa((virt_addr)fdt_addr), num_pages)) {
		kprintf("Failed to reserve FDT pages. Halting.\n");
		return false;
	}
	return true;
}

bool setup_page_allocator(const void *fdt_addr)
{
	if (!check_fdt(fdt_addr)) {
		kprintf("FDT validation failed. Halting.\n");
		return false;
	}

	memory_map_t mmap;
	// NOLINTNEXTLINE(*-int-to-ptr)
	if (get_mem(fdt_addr, &mmap) < 0) {
		kprintf("Failed to parse memory map from FDT. Halting.\n");
		return false;
	}

	for (int i = 0; i < mmap.count; i++) {
		bool page_init_result = page_allocator_add_region(
			mmap.regions[i].base, mmap.regions[i].size, true);

		if (!page_init_result) {
			kprintf("Failed to add memory region %d [0x%lx, size 0x%lx] to the page allocator\n",
				i, mmap.regions[i].base, mmap.regions[i].size);
			return false;
		}
	}

	if (!reserve_kernel_img_pages()) {
		kprintf("Error while reserving kernel binary pages\n");
		return false;
	}

	if (!reserve_fdt_pages(fdt_addr)) {
		kprintf("Error while reserving FDT pages\n");
		return false;
	}

	return true;
}

void print_kernelite_logo(void)
{
	kprintf(""
		"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
		"@@@@@@@@@@@@@@@@@@@@@@@@@@%%++%%@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
		"@@@@@@@@@@@@@@@@@@@@@@%%*=::::::=*%%@@@@@@@@@@@@@@@@@@@@@@\n"
		"@@@@@@@@@@@@@@@@@@@#+-::::::::::::-+#@@@@@@@@@@@@@@@@@@@\n"
		"@@@@@@@@@@@@@@@%%*=:::::::::--:::::::::=*%%@@@@@@@@@@@@@@@\n"
		"@@@@@@@@@@@#+-:::::::::--------:::::::::-+#@@@@@@@@@@@@@\n"
		"@@@@@@@@%%*=::::::::::--------------::::::::::=*%%@@@@@@@@\n"
		"@@@@@#+-:::::::::---------====---------:::::::::-+#@@@@@\n"
		"@#*=::::::::::--------===++++++===--------::::::::::=*#@\n"
		"@=::::::::--------===++++++++++++++===--------::::::::=@\n"
		"@=:::::--------==+++++++++****+++++++++==--------:::::=@\n"
		"@=:::::------=+++++++++*#%%%%%%%%%%%%#*+++++++++=------:::::=@\n"
		"@=:::::------=+++++**#%%%%%%%%%%%%%%%%%%%%%%%%#**+++++=------:::::=@\n"
		"@=:::::------=+++++#%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%#+++++=------:::::=@\n"
		"@=:::::------=+++++#%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%#+++++=------:::::=@\n"
		"@=:::::------=+++++#%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%#+++++=------:::::=@\n"
		"@=:::::------=+++++#%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%#+++++=------:::::=@\n"
		"@=:::::------=+++++#%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%#+++++=------:::::=@\n"
		"@=:::::------=+++++#%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%#+++++=------:::::=@\n"
		"@=:::::------=+++++#%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%#+++++=------:::::=@\n"
		"@=:::::------=+++++++*#%%%%%%%%%%%%%%%%%%%%#*+++++++=------:::::=@\n"
		"@=:::::------==+++++++++*##%%%%##*+++++++++==------:::::=@\n"
		"@=::::::--------===++++++++++++++++++===--------::::::=@\n"
		"@=::::::::::--------==++++++++++++==--------::::::::::=@\n"
		"@@%%*=::::::::::--------===++++===--------::::::::::=*%%@@\n"
		"@@@@@@#+-:::::::::---------==---------:::::::::-+#@@@@@@\n"
		"@@@@@@@@@%%*=::::::::::------------::::::::::=*%%@@@@@@@@@\n"
		"@@@@@@@@@@@@@#+-:::::::::------:::::::::-+#@@@@@@@@@@@@@\n"
		"@@@@@@@@@@@@@@@@@#=::::::::::::::::::=#@@@@@@@@@@@@@@@@@\n"
		"@@@@@@@@@@@@@@@@@@@@%%*=::::::::::=*%%@@@@@@@@@@@@@@@@@@@@\n"
		"@@@@@@@@@@@@@@@@@@@@@@@@#+-::-+#@@@@@@@@@@@@@@@@@@@@@@@@\n"
		"@@@@@@@@@@@@@@@@@@@@@@@@@@@##@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
		" _   __                     _ _ _       \n"
		"| | / /                    | (_) |      \n"
		"| |/ /  ___ _ __ _ __   ___| |_| |_ ___ \n"
		"|    \\ / _ \\ '__| '_ \\ / _ \\ | | __/ _ \\\n"
		"| |\\  \\  __/ |  | | | |  __/ | | ||  __/\n"
		"\\_| \\_/\\___|_|  |_| |_|\\___|_|_|\\__\\___|\n"
		"  a bare-metal kernel  |  github.com/Abhinkop/kernelite\n"
		"\n");
}

uint32_t get_core_id(void)
{
	mpidr_el1_t mpidr;
	READ_SYS_REG(mpidr_el1, mpidr.raw);
	return mpidr.aff0;
}

void print_current_el(void)
{
	current_el_t cur_el = read_current_el();

	const char *el_str;
	switch (cur_el.el) {
	case 0:
		el_str = "EL0 (Unprivileged / User)";
		break;
	case 1:
		el_str = "EL1 (Kernel)";
		break;
	case 2:
		el_str = "EL2 (Hypervisor)";
		break;
	case 3:
		el_str = "EL3 (Secure Monitor)";
		break;
	default:
		el_str = "Unknown";
	}

	kprintf("-------------- CurrentEL ----------------\n");
	kprintf("EL          : %u (%s)\n", cur_el.el, el_str);
	kprintf("Raw         : 0x%lx\n", cur_el.raw);
	kprintf("-----------------------------------------\n\n");
}
