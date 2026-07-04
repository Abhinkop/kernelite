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

#include "mem_layout/mem_layout.h"
#include "page_table/page_table.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief A null physical address value. */
#define PHYS_ADDR_NULL ((phy_addr)NULL)

/**
 * @brief Registers a new physical memory region with the allocator.
 *
 * Initializes a new memory chunk for the range starting at @p mem_start and
 * spanning @p mem_size bytes. If @p check_kernel_overlap is true, the region
 * is rejected when it overlaps the kernel image.
 *
 * @param mem_start The physical address of the beginning of the region.
 * @param mem_size  Total size of the region, in bytes.
 * @param check_kernel_overlap Whether to reject regions that overlap with
 * the kernel image.
 * @return true if the region was added successfully, false otherwise.
 */
bool page_allocator_add_region(phy_addr mem_start, size_t mem_size,
			       bool check_kernel_overlap);

/**
 * @brief Removes a previously registered memory region.
 *
 * Marks the region that starts at @p mem_start as no longer managed by the
 * allocator.
 *
 * @param mem_start The physical base address of the region to remove.
 * @return true if the region was found and removed, false otherwise.
 */
bool page_allocator_remove_region(phy_addr mem_start);

/** @brief Rewrites allocator bitmap pointers after the switch to high virtual
 * addresses. */
void fixup_page_allocator(void);

/**
 * @brief Reserves a specific number of contiguous pages.
 *
 * Marks the specified range of pages as reserved.
 *
 * @param start     The physical address of the first page to reserve.
 * @param num_pages The number of contiguous pages to reserve.
 * @return bool True if the pages were successfully reserved, false otherwise.
 */
bool reserve_page(phy_addr start, size_t num_pages);

/**
 * @brief Allocates a contiguous block of physical pages.
 *
 * Searches for a free span of memory large enough to hold @p num_pages.
 *
 * @param num_pages The number of contiguous 4 KiB pages requested.
 * @return phy_addr start address of the allocated block,
 * or PHYS_ADDR_NULL if insufficient contiguous memory exists.
 */
phy_addr page_alloc(size_t num_pages);

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

#ifdef RUN_TESTS

/**
 * @brief Marks every managed memory chunk as uninitialized.
 *
 * Test-only helper used to reset the allocator to a clean state between test
 * cases.
 */
void invalidate_all_memory_chunks(void);

/**
 * @brief Looks up the bitmap location and size for a managed address.
 *
 * Test-only helper that finds the chunk owning @p addr and reports its
 * bitmap. If no chunk owns @p addr, both outputs are set to 0.
 *
 * @param addr        Physical address owned by the chunk to inspect.
 * @param bitmap_addr Out-param receiving the bitmap's physical address.
 * @param bitmap_size Out-param receiving the bitmap size in bytes.
 */
void get_bitmap_addr_and_size(phy_addr addr, phy_addr *bitmap_addr,
			      size_t *bitmap_size);

#endif /* RUN_TESTS */

#endif /* ALLOCATOR_PAGE_ALLOCATOR_H */
