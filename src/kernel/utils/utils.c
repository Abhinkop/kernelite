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

bool reserve_kernel_img_pages(void)
{
	size_t img_size = get_image_size();
	size_t num_pages = (img_size + PAGE_SIZE - 1) / PAGE_SIZE;
	void *img_start = (void *)&image_start;

	kprintf("Reserving kernel image pages: start=%p, size=0x%lx bytes, pages=%u\n",
		img_start, img_size, num_pages);

	if (!reserve_page(img_start, num_pages)) {
		kprintf("Failed to reserve kernel image pages. Halting.\n");
		return false;
	}
	return true;
}

bool setup_global_allocator(const void *fdt_addr)
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
		page_init((void *)mmap.regions[0].base, mmap.regions[0].size);
	// NOLINTEND(*-int-to-ptr)

	if (!page_init_result) {
		kprintf("Failed to initialize page allocator. Halting.\n");
		return false;
	}

	if (!reserve_kernel_img_pages()) {
		kprintf("Error while reserving kernel binary pages\n");
		return false;
	}

	return true;
}
