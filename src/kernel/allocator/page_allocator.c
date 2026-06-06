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

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "utils/kprintf.h"
#include "linker/symbols.h"

/** @brief Start of the managed memory region */
static uint8_t *mem_base = NULL;

/** @brief Pointer to the bitmap (lives at mem_base) */
static uint8_t *bitmap = NULL;

/** @brief Total number of 4KB pages in the pool */
static size_t total_pages = 0;

bool page_init(void *mem_start, size_t mem_size)
{
	kprintf("PAGE: Initializing allocator at %p (size: %u bytes)\n",
		mem_start, mem_size);

	// Align the starting address to a 4KB boundary
	uintptr_t start_addr = (uintptr_t)mem_start;
	uintptr_t aligned_start = (start_addr + (PAGE_SIZE - 1)) &
				  ~(PAGE_SIZE - 1);

	if (aligned_start != start_addr) {
		kprintf("PAGE: Aligned start to 0x%lx (offset: %lu bytes)\n",
			aligned_start, aligned_start - start_addr);
	}

	size_t alignment_shift = aligned_start - start_addr;
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
		kprintf("PAGE ERROR: Bitmap size (%u bytes) exceeds available bitmap memory (%u bytes)\n",
			bitmap_size_bytes, BITMAP_SIZE);
		return false;
	}

	kprintf("PAGE: Total pages: %u, Bitmap requires: %u bytes (%u pages)\n",
		total_pages, bitmap_size_bytes,
		(bitmap_size_bytes + PAGE_SIZE - 1) / PAGE_SIZE);

	uintptr_t bitmap_start_addr = (uintptr_t)&page_allocator_bit_map_start;
	kprintf("PAGE: Bit map starts at %lx and ends at %lx\n",
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

bool reserve_page(void *ptr, size_t num_pages)
{
	if (!ptr || !mem_base || total_pages == 0 || !bitmap) {
		return false;
	}

	uintptr_t start_addr = (uintptr_t)ptr;
	uintptr_t end_addr = start_addr + (num_pages * PAGE_SIZE);

	uintptr_t mem_start = (uintptr_t)mem_base;
	uintptr_t mem_end = mem_start + (total_pages * PAGE_SIZE);

	if (start_addr < mem_start || end_addr > mem_end) {
		kprintf("PAGE ERROR: Attempted to reserve out-of-bounds range %p - %p\n",
			ptr, end_addr);
		return false;
	}

	size_t start_index = ((uint8_t *)ptr - mem_base) / PAGE_SIZE;
	for (size_t i = 0; i < num_pages; i++) {
		size_t page_index = start_index + i;
		bitmap[page_index / 8] |= (1 << (page_index % 8));
	}
	kprintf("PAGE: Reserved %u pages at %p (index %u)\n", num_pages, ptr,
		start_index);
	return true;
}

void *page_alloc(size_t num_pages)
{
	if (num_pages == 0) {
		return NULL;
	}

	if (num_pages > total_pages) {
		kprintf("PAGE: Allocation failed. Request (%u) exceeds total capacity (%u)\n",
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
				kprintf("PAGE: Allocated %u pages at %p\n",
					num_pages, ptr);
				return ptr;
			}
		} else {
			continuous_found = 0;
		}
	}

	kprintf("PAGE: Allocation failed. No contiguous block of %u pages found.\n",
		num_pages);
	return NULL;
}

void page_free(void *ptr, size_t num_pages)
{
	if (!ptr)
		return;

	if (ptr < (void *)mem_base ||
	    ptr >= (void *)(mem_base + (total_pages * PAGE_SIZE))) {
		kprintf("PAGE ERROR: Attempted to free out-of-bounds pointer %p\n",
			ptr);
		return;
	}

	// Calculate start index
	size_t start_index = ((uint8_t *)ptr - mem_base) / PAGE_SIZE;

	// Boundary check for the range
	if (start_index + num_pages > total_pages) {
		kprintf("PAGE WARNING: Free range out of bounds. Truncating %u to %u pages\n",
			num_pages, total_pages - start_index);
		num_pages = total_pages - start_index;
	}

	kprintf("PAGE: Freeing %u pages at %p\n", num_pages, ptr);

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
	kprintf("Managed Range: 0x%lx - 0x%lx (%u pages)\n",
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

			kprintf("  [0x%lx - 0x%lx] %s (%u pages)\n",
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
