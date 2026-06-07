/**
 * @file page_allocator.h
 * @brief Physical page allocator for managing system memory blocks.
 *
 * This module handles allocation and deallocation of fixed-size 4 KiB pages,
 * tracks page usage with a bitmap, and exposes the kernel page allocator API.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-16
 */

#ifndef ALLOCATOR_PAGE_ALLOCATOR_H
#define ALLOCATOR_PAGE_ALLOCATOR_H

#include "page_table/page_table.h"
#include "linker/linker_defines.h"
#include "mem_layout/mem_layout.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/** @brief Size of the bitmap in bytes. */
#define BITMAP_SIZE LINKER_BITMAP_SIZE

/**
 * @brief Initializes the page allocator with a specific region of memory.
 *
 * Sets up the internal tracking structures (e.g., bitmap or free list) to
 * manage the physical memory starting at @p mem_start.
 *
 * @param mem_start Pointer to the beginning of the manageable physical memory.
 * @param mem_size  Total size of the memory region in bytes.
 * @return bool True if initialization was successful, false otherwise.
 */
bool page_init(void *mem_start, size_t mem_size);

/** @brief Fixes up the page allocator after the switch to high VAs. */
void fixup_page_allocator(void);

/**
 * @brief Reserves a specific number of contiguous pages.
 *
 * Marks the specified range of pages as reserved.
 *
 * @param ptr       Pointer to the start of the memory block to reserve.
 * @param num_pages The number of contiguous pages to reserve.
 * @return bool True if the pages were successfully reserved, false otherwise.
 */
bool reserve_page(void *ptr, size_t num_pages);

/**
 * @brief Allocates a contiguous block of physical pages.
 *
 * Searches for a free span of memory large enough to hold @p num_pages.
 *
 * @param num_pages The number of contiguous 4 KiB pages requested.
 * @return void* Pointer to the start of the allocated block,
 * or NULL if insufficient contiguous memory exists.
 */
void *page_alloc(size_t num_pages);

/**
 * @brief Frees a previously allocated block of physical pages.
 *
 * Marks the specified range of pages as available for future allocations.
 *
 * @param start     The physical address of the first page to free.
 * @param num_pages The number of contiguous pages to release.
 */
void page_free(phy_addr start, size_t num_pages);

/**
 * @brief Scans the bitmap and prints the status of all memory regions.
 *
 * Iterates through the managed page pool and groups contiguous pages with
 * the same status (FREE or USED) into blocks for concise UART output.
 */
void page_dump_status(void);

#endif /* ALLOCATOR_PAGE_ALLOCATOR_H */
