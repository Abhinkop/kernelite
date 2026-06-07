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
#include "utils/kprintf.h"
#include "allocator/page_allocator.h"
#include "linker/symbols.h"
#include "fdt/fdt.h"

#include <libfdt.h>

/**
 * @brief Reserve pages occupied by the kernel image.
 *
 * This function calculates the number of pages occupied by the kernel
 * image based on the linker-provided symbols and reserves those pages in
 * the page allocator to prevent them from being allocated for other purposes.
 *
 * @return bool True if reservation was successful, false otherwise.
 */
bool reserve_kernel_img_pages(void)
{
	size_t img_size = get_image_size();
	size_t num_pages = (img_size + PAGE_SIZE - 1) / PAGE_SIZE;
	void *img_start = (void *)&image_start;

	kprintf("Reserving kernel image pages: start=%p, size=0x%lx bytes, pages=%u\n",
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
 * @param fdt_addr Pointer to the FDT address.
 * @return bool True if reservation was successful, false otherwise.
 */
bool reserve_fdt_pages(const void *fdt_addr)
{
	size_t fdt_size = fdt_totalsize(fdt_addr);
	size_t num_pages = (fdt_size + PAGE_SIZE - 1) / PAGE_SIZE;

	kprintf("Reserving FDT pages: start=%p, size=0x%lx bytes, pages=%u\n",
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

	Memory_map_t mmap;
	// NOLINTNEXTLINE(*-int-to-ptr)
	if (get_mem(fdt_addr, &mmap) < 0) {
		kprintf("Failed to parse memory map from FDT. Halting.\n");
		return false;
	}

	if (mmap.count != 1) {
		kprintf("Current implementation only supports a single memory region. Halting.\n");
		return false;
	}

	// NOLINTBEGIN(*-int-to-ptr)
	bool page_init_result =
		page_init(mmap.regions[0].base, mmap.regions[0].size);
	// NOLINTEND(*-int-to-ptr)

	if (!page_init_result) {
		kprintf("Failed to initialize page allocator. Halting.\n");
		return false;
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
