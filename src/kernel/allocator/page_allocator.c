/**
 * @file page_allocator.c
 * @brief Physical page allocator implementation using a bitmap.
 *
 * This module implements a simple physical page allocator backed by a
 * linker-reserved bitmap region. It provides initialization, reservation,
 * allocation, freeing and status dump helpers used by the kernel early
 * memory management.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-16
 */

#include "allocator/page_allocator.h"

#include "linker/linker_defines.h"
#include "linker/symbols.h"
#include "utils/kprintf.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Size of the bitmap in bytes. */
#define BITMAP_SIZE LINKER_BITMAP_SIZE

/** @brief Start of the managed memory region */
static uint8_t *mem_base = NULL;

/** @brief Pointer to the bitmap (lives at mem_base) */
static uint8_t *bitmap = NULL;

/** @brief Total number of 4KB pages in the pool */
static size_t total_pages = 0;

bool page_init(phy_addr mem_start, size_t mem_size)
{
	kprintf("PAGE: Initializing allocator at 0x%lx (size: 0x%lx bytes)\n",
		mem_start, mem_size);

	// Align the starting address to a 4KB boundary
	uintptr_t aligned_start = (mem_start + (PAGE_SIZE - 1)) &
				  ~(PAGE_SIZE - 1);

	if (aligned_start != mem_start) {
		kprintf("PAGE: Aligned start to 0x%lx (offset: %lu bytes)\n",
			aligned_start, aligned_start - mem_start);
	}

	size_t alignment_shift = aligned_start - mem_start;
	if (alignment_shift >= mem_size) {
		kprintf("PAGE ERROR: Alignment shift (%lx bytes) exceeds total memory size (%lx bytes)\n",
			alignment_shift, mem_size);
		return false;
	}

	// Adjust size based on the alignment shift
	mem_size -= alignment_shift;

	// NOLINTNEXTLINE(*-int-to-ptr)
	mem_base = (uint8_t *)aligned_start;
	total_pages = mem_size / PAGE_SIZE;

	// 2. Calculate bitmap requirements
	size_t bitmap_size_bytes = (total_pages + 7) / 8;

	if (bitmap_size_bytes > BITMAP_SIZE) {
		kprintf("PAGE ERROR: Bitmap size (0x%lx bytes) exceeds available bitmap memory (0x%x bytes)\n",
			bitmap_size_bytes, BITMAP_SIZE);
		return false;
	}

	kprintf("PAGE: Total pages: 0x%lx, Bitmap requires: 0x%lx bytes (0x%lx pages)\n",
		total_pages, bitmap_size_bytes,
		(bitmap_size_bytes + PAGE_SIZE - 1) / PAGE_SIZE);

	uintptr_t bitmap_start_addr = (uintptr_t)&page_allocator_bit_map_start;
	kprintf("PAGE: Bit map starts at 0x%lx and ends at 0x%lx\n",
		bitmap_start_addr, bitmap_start_addr + BITMAP_SIZE);

	// NOLINTNEXTLINE(*-int-to-ptr)
	bitmap = (uint8_t *)bitmap_start_addr;

	// 3. Zero out the bitmap (all pages initially free)
	for (size_t i = 0; i < bitmap_size_bytes; i++) {
		bitmap[i] = 0;
	}

	kprintf("PAGE: Usable memory starts at 0x%lx\n", (uintptr_t)mem_base);
	kprintf("PAGE: Usable memory ends at 0x%lx\n",
		(uintptr_t)mem_base + mem_size - 1);
	return true;
}

void fixup_page_allocator(void)
{
	// After the initial setup, we need to convert the physical addresses to
	// virtual addresses for the bitmap and the managed memory region,  as
	// the kernel operates in the high virtual address space. This function
	// should be called after the identity map is set up and the MMU is
	// enabled and the switch to high VA is done in the main function, to
	// ensure the page allocator can continue functioning correctly with the
	// new virtual addresses. The bitmap and mem_base should be updated to
	// point to their corresponding virtual addresses after the switch,
	// which are expected to be at a fixed offset from their physical
	// addresses defined by the kernel base.

	// NOLINTNEXTLINE(*-int-to-ptr)
	mem_base = (uint8_t *)pa_to_va((phy_addr)mem_base);
	// NOLINTNEXTLINE(*-int-to-ptr)
	bitmap = (uint8_t *)pa_to_va((phy_addr)bitmap);
}

bool reserve_page(phy_addr start, size_t num_pages)
{
	if (!mem_base || total_pages == 0 || !bitmap) {
		return false;
	}

	phy_addr start_addr = start;
	phy_addr end_addr = start_addr + (num_pages * PAGE_SIZE);

	phy_addr mem_start = va_to_pa((virt_addr)mem_base);
	phy_addr mem_end = mem_start + (total_pages * PAGE_SIZE);

	if (start_addr < mem_start || end_addr > mem_end) {
		kprintf("PAGE ERROR: Attempted to reserve out-of-bounds range 0x%lx - 0x%lx\n",
			start, end_addr);
		return false;
	}

	size_t start_index = (start - mem_start) / PAGE_SIZE;
	for (size_t i = 0; i < num_pages; i++) {
		size_t page_index = start_index + i;
		bitmap[page_index / 8] |= (1 << (page_index % 8));
	}
	kprintf("PAGE: Reserved 0x%lx pages at 0x%lx (index 0x%lx)\n",
		num_pages, start, start_index);
	return true;
}

void *page_alloc(size_t num_pages)
{
	if (num_pages == 0) {
		return NULL;
	}

	if (num_pages > total_pages) {
		kprintf("PAGE: Allocation failed. Request (0x%lx) exceeds total capacity (0x%lx)\n",
			num_pages, total_pages);
		return NULL;
	}

	size_t continuous_found = 0;
	size_t start_index = 0;

	for (size_t i = 0; i < total_pages; i++) {
		uint8_t is_used = bitmap[i / 8] & (1 << (i % 8));

		if (!is_used) {
			if (continuous_found == 0) {
				start_index = i;
			}
			continuous_found++;

			if (continuous_found == num_pages) {
				// Mark pages as used
				for (size_t j = start_index;
				     j < start_index + num_pages; j++) {
					bitmap[j / 8] |= (1 << (j % 8));
				}

				void *ptr = (void *)(mem_base +
						     (start_index * PAGE_SIZE));
				kprintf("PAGE: Allocated 0x%lx pages at %p\n",
					num_pages, ptr);
				return ptr;
			}
		} else {
			continuous_found = 0;
		}
	}

	kprintf("PAGE: Allocation failed. No contiguous block of 0x%lx pages found.\n",
		num_pages);
	return NULL;
}

void page_free(phy_addr start, size_t num_pages)
{
	if (!mem_base || total_pages == 0 || !bitmap) {
		return;
	}

	phy_addr mem_start = va_to_pa((virt_addr)mem_base);
	phy_addr mem_end =
		va_to_pa((virt_addr)(mem_base + (total_pages * PAGE_SIZE)));
	if (start < mem_start || start >= mem_end) {
		kprintf("PAGE ERROR: Attempted to free out-of-bounds pointer 0x%lx\n",
			start);
		return;
	}

	// Calculate start index
	size_t start_index = (start - mem_start) / PAGE_SIZE;

	// Boundary check for the range
	if (start_index + num_pages > total_pages) {
		kprintf("PAGE WARNING: Free range out of bounds. Truncating 0x%lx to 0x%lx pages\n",
			num_pages, total_pages - start_index);
		num_pages = total_pages - start_index;
	}

	kprintf("PAGE: Freeing 0x%lx pages at 0x%lx\n", num_pages, start);

	// Mark pages as free
	for (size_t i = start_index; i < start_index + num_pages; i++) {
		bitmap[i / 8] &= ~(1 << (i % 8));
	}
}

void page_dump_status(void)
{
	if (total_pages == 0) {
		kprintf("Page Allocator: Not initialized\n");
		return;
	}

	kprintf("--- Physical Page Dump ---\n");
	kprintf("Managed Range: 0x%lx - 0x%lx (0x%lx pages)\n",
		(uintptr_t)mem_base,
		(uintptr_t)mem_base + (total_pages * PAGE_SIZE), total_pages);

	size_t start_idx = 0;
	// Get status of the first page to initialize the tracker
	uint8_t current_status = (bitmap[0] & (1 << 0)) ? 1 : 0;

	for (size_t i = 1; i <= total_pages; i++) {
		uint8_t status = 0;

		// If we haven't reached the end, check the current bit
		if (i < total_pages) {
			status = (bitmap[i / 8] & (1 << (i % 8))) ? 1 : 0;
		}

		// If status changed OR we reached the end of the bitmap
		if (status != current_status || i == total_pages) {
			uintptr_t block_start =
				(uintptr_t)mem_base + (start_idx * PAGE_SIZE);
			uintptr_t block_end =
				(uintptr_t)mem_base + (i * PAGE_SIZE);
			size_t block_size_pages = i - start_idx;

			kprintf("  [0x%lx - 0x%lx] %s (0x%lx pages)\n",
				block_start,
				block_end - 1, // Inclusive range display
				current_status ? "USED" : "FREE",
				block_size_pages);

			// Update tracker for the next block
			start_idx = i;
			current_status = status;
		}
	}
	kprintf("--- End of Dump ---\n");
}
