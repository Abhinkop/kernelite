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

/** @brief Maximum number of memory chunks to manage */
#define MAX_MEM_CHUNKS 10

/**
 * @brief State for one managed physical-memory chunk.
 */
typedef struct memory_chunk_t {
	/** @brief Pointer to the bitmap tracking the chunk's pages. */
	uint8_t *bitmap;
	/** @brief Number of managed pages in this chunk, excluding the bitmap.
	 */
	size_t total_pages;
	/** @brief Base address of the managed physical-memory range. */
	phy_addr mem_base;
	/** @brief Whether this chunk has been initialized. */
	bool initialized;
} memory_chunk_t;

/**
 * @brief Array of memory chunks managed by the allocator.
 */
static memory_chunk_t mem_chunks[MAX_MEM_CHUNKS];

/**
 * @brief Determine whether a region overlaps the kernel image.
 *
 * @param mem_start Physical base address of the region to inspect.
 * @param mem_size  Size of the region in bytes.
 * @return true when the region overlaps the linked kernel image, false
 * otherwise.
 */
static bool check_kernel_binary_overlap(phy_addr mem_start, size_t mem_size)
{
	phy_addr kernel_start = (phy_addr)&image_start;
	phy_addr kernel_end = (phy_addr)&image_end;

	phy_addr mem_end = mem_start + mem_size;

	if (kernel_start < mem_end && mem_start < kernel_end) {
		kprintf("PAGE ERROR: Memory region [0x%lx, 0x%lx) overlaps with kernel binary [0x%lx, 0x%lx)\n",
			mem_start, mem_end, kernel_start, kernel_end);
		return true;
	}

	return false;
}

/**
 * @brief Mark a range of pages as reserved inside a chunk.
 *
 * @param start     Physical base address of the range to reserve.
 * @param num_pages Number of contiguous pages to reserve.
 * @param chunk     Destination chunk that owns the range.
 * @return true when the reservation succeeded, false otherwise.
 */
static bool reserve_page_chunk(phy_addr start, size_t num_pages,
			       memory_chunk_t *chunk)
{
	if (!chunk->mem_base || chunk->total_pages == 0 || !chunk->bitmap) {
		return false;
	}

	phy_addr start_addr = start;
	phy_addr end_addr = start_addr + (num_pages * PAGE_SIZE);

	phy_addr mem_start = chunk->mem_base;
	phy_addr mem_end = mem_start + (chunk->total_pages * PAGE_SIZE);

	if (start_addr < mem_start || end_addr > mem_end) {
		kprintf("PAGE ERROR: Attempted to reserve out-of-bounds range 0x%lx - 0x%lx\n",
			start, end_addr);
		return false;
	}

	size_t start_index = (start - mem_start) / PAGE_SIZE;
	for (size_t i = 0; i < num_pages; i++) {
		size_t page_index = start_index + i;
		chunk->bitmap[page_index / 8] |= (1 << (page_index % 8));
	}
	kprintf("PAGE: Reserved 0x%lx pages at 0x%lx (index 0x%lx)\n",
		num_pages, start, start_index);
	return true;
}

/**
 * @brief Initialize one memory chunk from a physical-memory region.
 *
 * @param mem_start           Physical base address of the region.
 * @param mem_size            Size of the region in bytes.
 * @param chunk               Chunk state to populate.
 * @param check_kernel_overlap Whether to reject regions overlapping the
 * kernel image.
 * @return true when the chunk was initialized successfully, false otherwise.
 */
static bool page_init_chunk(phy_addr mem_start, size_t mem_size,
			    memory_chunk_t *chunk, bool check_kernel_overlap)
{
	chunk->initialized = false;

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
	chunk->mem_base = aligned_start;
	size_t total_pages = mem_size / PAGE_SIZE;

	// 2. Calculate bitmap requirements
	const size_t bitmap_size_bytes = (total_pages + 7) / 8;
	const size_t bitmap_size_pages =
		(bitmap_size_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

	// check if the bitmap region overlaps with the kernel binary
	if (check_kernel_overlap &&
	    check_kernel_binary_overlap(aligned_start,
					bitmap_size_pages * PAGE_SIZE)) {
		kprintf("PAGE ERROR: Bitmap region overlaps with kernel binary\n");
		return false;
	}

	// place the bitmap at the start of the managed memory region
	// NOLINTNEXTLINE(*-int-to-ptr)
	chunk->bitmap = (uint8_t *)pa_to_va(aligned_start);

	kprintf("PAGE: Total pages: 0x%lx, Bitmap requires: 0x%lx bytes (0x%lx pages)\n",
		total_pages, bitmap_size_bytes, bitmap_size_pages);

	// 3. Zero out the bitmap (all pages initially free)
	for (size_t i = 0; i < bitmap_size_bytes; i++) {
		chunk->bitmap[i] = 0;
	}

	chunk->total_pages = total_pages;

	if (!reserve_page_chunk(va_to_pa((virt_addr)chunk->bitmap),
				bitmap_size_pages, chunk)) {
		kprintf("PAGE ERROR: Failed to reserve bitmap pages\n");
		return false;
	}

	const phy_addr usable_start =
		chunk->mem_base + bitmap_size_pages * PAGE_SIZE;
	const phy_addr usable_end = chunk->mem_base + mem_size;
	kprintf("PAGE: Usable memory starts at 0x%lx\n",
		(uintptr_t)usable_start);
	kprintf("PAGE: Usable memory ends at 0x%lx\n",
		(uintptr_t)usable_end - 1);
	chunk->initialized = true;
	return true;
}

/**
 * @brief Allocate a contiguous range of pages from a single chunk.
 *
 * @param num_pages Number of contiguous pages to allocate.
 * @param chunk     Chunk to search for free pages.
 * @return Pointer to the allocation base, or NULL if none was found.
 */
static void *page_alloc_chunk(size_t num_pages, memory_chunk_t *chunk)
{
	if (num_pages == 0) {
		return NULL;
	}

	if (num_pages > chunk->total_pages) {
		kprintf("PAGE: Allocation failed. Request (0x%lx) exceeds total capacity (0x%lx)\n",
			num_pages, chunk->total_pages);
		return NULL;
	}

	size_t continuous_found = 0;
	size_t start_index = 0;

	for (size_t i = 0; i < chunk->total_pages; i++) {
		uint8_t is_used = chunk->bitmap[i / 8] & (1 << (i % 8));

		if (!is_used) {
			if (continuous_found == 0) {
				start_index = i;
			}
			continuous_found++;

			if (continuous_found == num_pages) {
				// Mark pages as used
				for (size_t j = start_index;
				     j < start_index + num_pages; j++) {
					chunk->bitmap[j / 8] |= (1 << (j % 8));
				}

				// NOLINTNEXTLINE(*-int-to-ptr)
				void *ptr = (void *)(chunk->mem_base +
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

/**
 * @brief Mark a previously allocated range of pages as free.
 *
 * @param start     Physical base address of the range to free.
 * @param num_pages Number of contiguous pages to free.
 * @param chunk     Chunk that owns the range.
 */
static void page_free_chunk(phy_addr start, size_t num_pages,
			    memory_chunk_t *chunk)
{
	if (!chunk->mem_base || chunk->total_pages == 0 || !chunk->bitmap) {
		return;
	}

	phy_addr mem_start = chunk->mem_base;
	phy_addr mem_end = chunk->mem_base + (chunk->total_pages * PAGE_SIZE);
	if (start < mem_start || start >= mem_end) {
		kprintf("PAGE ERROR: Attempted to free out-of-bounds pointer 0x%lx\n",
			start);
		return;
	}

	// Calculate start index
	size_t start_index = (start - mem_start) / PAGE_SIZE;

	// Boundary check for the range
	if (start_index + num_pages > chunk->total_pages) {
		kprintf("PAGE WARNING: Free range out of bounds. Truncating 0x%lx to 0x%lx pages\n",
			num_pages, chunk->total_pages - start_index);
		num_pages = chunk->total_pages - start_index;
	}

	kprintf("PAGE: Freeing 0x%lx pages at 0x%lx\n", num_pages, start);

	// Mark pages as free
	for (size_t i = start_index; i < start_index + num_pages; i++) {
		chunk->bitmap[i / 8] &= ~(1 << (i % 8));
	}
}

/**
 * @brief Dump the allocation state for one chunk.
 *
 * @param chunk Chunk whose page state should be printed.
 */
static void page_dump_status_chunk(memory_chunk_t *chunk)
{
	if (chunk->total_pages == 0) {
		kprintf("Page Allocator: Not initialized\n");
		return;
	}

	kprintf("--- Physical Page Dump ---\n");
	kprintf("Managed Range: 0x%lx - 0x%lx (0x%lx pages)\n",
		(uintptr_t)chunk->mem_base,
		(uintptr_t)chunk->mem_base + (chunk->total_pages * PAGE_SIZE),
		chunk->total_pages);

	size_t start_idx = 0;
	// Get status of the first page to initialize the tracker
	uint8_t current_status = (chunk->bitmap[0] & (1 << 0)) ? 1 : 0;

	for (size_t i = 1; i <= chunk->total_pages; i++) {
		uint8_t status = 0;

		// If we haven't reached the end, check the current bit
		if (i < chunk->total_pages) {
			status = (chunk->bitmap[i / 8] & (1 << (i % 8))) ? 1 :
									   0;
		}

		// If status changed OR we reached the end of the bitmap
		if (status != current_status || i == chunk->total_pages) {
			uintptr_t block_start = (uintptr_t)chunk->mem_base +
						(start_idx * PAGE_SIZE);
			uintptr_t block_end =
				(uintptr_t)chunk->mem_base + (i * PAGE_SIZE);
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

/**
 * @brief Find the chunk that contains a given physical address.
 *
 * @param addr Physical address to locate.
 * @return Pointer to the owning chunk, or NULL if none matches.
 */
static memory_chunk_t *find_chunk_for_address(phy_addr addr)
{
	for (size_t i = 0; i < MAX_MEM_CHUNKS; i++) {
		if (mem_chunks[i].initialized) {
			phy_addr chunk_start = mem_chunks[i].mem_base;
			phy_addr chunk_end =
				chunk_start +
				(mem_chunks[i].total_pages * PAGE_SIZE);
			if (addr >= chunk_start && addr < chunk_end) {
				return &mem_chunks[i];
			}
		}
	}
	return NULL;
}

/**
 * @brief Find a chunk with a contiguous free range of the requested size.
 *
 * @param num_pages Minimum contiguous free page count required.
 * @return Pointer to a suitable chunk, or NULL if none was found.
 */
static memory_chunk_t *find_chunk_with_free_pages(size_t num_pages)
{
	for (size_t i = 0; i < MAX_MEM_CHUNKS; i++) {
		if (mem_chunks[i].initialized) {
			size_t continuous_found = 0;
			for (size_t j = 0; j < mem_chunks[i].total_pages; j++) {
				uint8_t is_used = mem_chunks[i].bitmap[j / 8] &
						  (1 << (j % 8));
				if (!is_used) {
					continuous_found++;
					if (continuous_found == num_pages) {
						return &mem_chunks[i];
					}
				} else {
					continuous_found = 0;
				}
			}
		}
	}
	return NULL;
}

bool page_allocator_add_region(phy_addr mem_start, size_t mem_size,
			       bool check_kernel_overlap)
{
	for (size_t i = 0; i < MAX_MEM_CHUNKS; i++) {
		if (mem_chunks[i].mem_base == mem_start ||
		    !mem_chunks[i].initialized) {
			if (!page_init_chunk(mem_start, mem_size,
					     &mem_chunks[i],
					     check_kernel_overlap)) {
				kprintf("PAGE ERROR: Failed to initialize memory chunk at 0x%lx\n",
					mem_start);
				return false;
			}
			kprintf("PAGE: Added new memory region at 0x%lx (size: 0x%lx bytes)\n",
				mem_start, mem_size);
			return true;
		}
	}

	kprintf("PAGE ERROR: Maximum number of memory chunks (%d) reached. Cannot add new region.\n",
		MAX_MEM_CHUNKS);
	return false;
}

bool page_allocator_remove_region(phy_addr mem_start)
{
	for (size_t i = 0; i < MAX_MEM_CHUNKS; i++) {
		if (mem_chunks[i].initialized &&
		    mem_chunks[i].mem_base == mem_start) {
			mem_chunks[i].initialized = false;
			kprintf("PAGE: Removed memory region at 0x%lx\n",
				mem_start);
			return true;
		}
	}

	kprintf("PAGE ERROR: Memory region at 0x%lx not found. Cannot remove.\n",
		mem_start);
	return false;
}

void fixup_page_allocator(void)
{
	// After the initial setup, we need to convert each chunk's bitmap
	// pointer from a physical to a virtual address, as the kernel operates
	// in the high virtual address space. This function should be called
	// after the identity map is set up and the MMU is enabled and the
	// switch to high VA is done in the main function, to ensure the page
	// allocator can continue functioning correctly with the new virtual
	// addresses. Each chunk's bitmap pointer is updated to its
	// corresponding virtual address after the switch, which is expected to
	// be at a fixed offset from its physical address defined by the kernel
	// base. The chunk mem_base values stay physical and are not converted.

	for (size_t i = 0; i < MAX_MEM_CHUNKS; i++) {
		if (mem_chunks[i].initialized) {
			// NOLINTNEXTLINE(*-int-to-ptr)
			mem_chunks[i].bitmap = (uint8_t *)pa_to_va(
				(phy_addr)mem_chunks[i].bitmap);
		}
	}
}

bool reserve_page(phy_addr start, size_t num_pages)
{
	memory_chunk_t *chunk = find_chunk_for_address(start);
	if (!chunk) {
		kprintf("PAGE ERROR: No memory chunk found for address 0x%lx\n",
			start);
		return false;
	}
	return reserve_page_chunk(start, num_pages, chunk);
}

void *page_alloc(size_t num_pages)
{
	memory_chunk_t *chunk = find_chunk_with_free_pages(num_pages);
	if (!chunk) {
		kprintf("PAGE ERROR: No memory chunk has enough free pages for allocation of 0x%lx pages\n",
			num_pages);
		return NULL;
	}

	return page_alloc_chunk(num_pages, chunk);
}

void page_free(phy_addr start, size_t num_pages)
{
	memory_chunk_t *chunk = find_chunk_for_address(start);
	if (!chunk) {
		kprintf("PAGE ERROR: No memory chunk found for address 0x%lx\n",
			start);
		return;
	}
	page_free_chunk(start, num_pages, chunk);
}

void page_dump_status(void)
{
	for (size_t i = 0; i < MAX_MEM_CHUNKS; i++) {
		if (mem_chunks[i].initialized) {
			kprintf("\n--- Memory Chunk %zu ---\n", i);
			page_dump_status_chunk(&mem_chunks[i]);
		}
	}
}
