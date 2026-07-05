/**
 * @file kalloc.c
 * @brief Kernel heap allocator implementation.
 *
 * This module implements the kernel-side dynamic memory API declared in
 * kalloc.h. It hands out variable-sized allocations from the kernel heap and
 * returns them for reuse, layering on top of the physical page allocator.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-07-05
 */

#include "allocator/kalloc.h"

#include "allocator/page_allocator.h"
#include "page_table/page_table.h"
#include "utils/kprintf.h"
#include "utils/string.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Number of block descriptors that fit in a single management page.
 *
 * A management page holds an array of ::allocated_block_desc_t plus a pointer
 * to the next page; this is how many descriptors fill the remaining space.
 */
#define MAX_MANAGEMENT_BLOCKS_PER_PAGE                              \
	((PAGE_SIZE - sizeof(struct kalloc_management_pages_t *)) / \
	 sizeof(allocated_block_desc_t))

/**
 * @brief Bookkeeping descriptor for a single kmalloc() allocation.
 */
typedef struct allocated_block_desc_t {
	void *ptr; /**< Virtual address of the allocated block. */
	size_t size; /**< Requested allocation size, in bytes. */
	bool is_used; /**< Whether this descriptor is currently in use. */
	struct allocated_block_desc_t *next; /**< Next descriptor in the list.
					      */
} allocated_block_desc_t;

/**
 * @brief A page of descriptors used to track live kmalloc() allocations.
 */
typedef struct kalloc_management_pages_t {
	/** @brief Descriptor slots stored in this page. */
	allocated_block_desc_t page[MAX_MANAGEMENT_BLOCKS_PER_PAGE];
	/** @brief Next management page in the chain, or NULL. */
	struct kalloc_management_pages_t *next_page;
} kalloc_management_pages_t;

/** @brief Head of the linked list of live allocation descriptors. */
static allocated_block_desc_t *allocated = NULL;

/** @brief Head of the linked list of management pages. */
static kalloc_management_pages_t *kalloc_page_head = NULL;

/**
 * @brief Allocates and zero-initializes a new management page.
 *
 * Reserves one physical page for storing allocation descriptors and maps it to
 * its kernel virtual address.
 *
 * @return Pointer to the new management page, or NULL on failure.
 */
static kalloc_management_pages_t *alloc_management_page(void)
{
	phy_addr p_addr = page_alloc(1);
	if (p_addr == PHYS_ADDR_NULL) {
		return NULL;
	}

	kalloc_management_pages_t *v_addr =
		// NOLINTNEXTLINE(*-int-to-ptr)
		(kalloc_management_pages_t *)pa_to_va(p_addr);
	if (v_addr == NULL) {
		return NULL;
	}
	int ret = memset_s(v_addr, PAGE_SIZE, 0, PAGE_SIZE);
	if (ret != 0) {
		kprintf("Failed to zero-initialize management page\n");
		page_free(p_addr, 1);
		return NULL;
	}
	return v_addr;
}

/**
 * @brief Finds an unused block descriptor, growing the pool if needed.
 *
 * Scans the existing management pages for a free descriptor and marks it used.
 * If none are free, a new management page is allocated and appended.
 *
 * @return Pointer to a reserved block descriptor, or NULL on failure.
 */
static allocated_block_desc_t *find_free_management_block(void)
{
	if (kalloc_page_head == NULL) {
		// Allocate a new page for management

		kalloc_management_pages_t *page = alloc_management_page();
		if (page == NULL) {
			kprintf("Failed to allocate management page for kalloc\n");
			return NULL;
		}
		kalloc_page_head = page;
		page->next_page = NULL;
	}

	kalloc_management_pages_t *current = kalloc_page_head;
	kalloc_management_pages_t *prev = NULL;
	while (current != NULL) {
		for (size_t index = 0; index < MAX_MANAGEMENT_BLOCKS_PER_PAGE;
		     index++) {
			if (current->page[index].is_used == false) {
				current->page[index].is_used = true;
				return &current->page[index];
			}
		}
		prev = current;
		current = current->next_page;
	}
	if (prev != NULL) {
		kalloc_management_pages_t *new_page = alloc_management_page();
		if (new_page == NULL) {
			kprintf("Failed to allocate management page for kalloc\n");
			return NULL;
		}
		prev->next_page = new_page;
		new_page->next_page = NULL;
		new_page->page[0].is_used = true;
		return &new_page->page[0];
	}
	kprintf("Failed to find free management block for kalloc\n");
	return NULL;
}

/**
 * @brief Returns a management page to the page allocator once it is empty.
 *
 * Locates the management page holding @p removed by masking the descriptor
 * address down to its page base. If none of that page's descriptors are still
 * in use, the page is unlinked from the management-page chain and its physical
 * page is freed.
 *
 * @param removed A descriptor that was just marked free. A NULL @p removed is
 * ignored.
 */
static void free_management_page_if_empty(allocated_block_desc_t *removed)
{
	if (removed == NULL) {
		return;
	}
	uintptr_t page_addr = (uintptr_t)removed & ~((uintptr_t)PAGE_SIZE - 1);
	kalloc_management_pages_t *current = kalloc_page_head;
	kalloc_management_pages_t *prev = kalloc_page_head;
	while (current != NULL) {
		// NOLINTNEXTLINE(*-int-to-ptr)
		if (current != (kalloc_management_pages_t *)page_addr) {
			prev = current;
			current = current->next_page;
			continue;
		}
		for (size_t index = 0; index < MAX_MANAGEMENT_BLOCKS_PER_PAGE;
		     index++) {
			if (current->page[index].is_used) {
				/* Can't remove this page, it has live
				 * allocations */
				return;
			}
		}
		break;
	}

	if (current == NULL) {
		kprintf("Failed to find management page for freed block\n");
		return;
	}

	/* This page has no live allocations, so it can be freed */
	if (prev == current) {
		kalloc_page_head = current->next_page;
	} else {
		prev->next_page = current->next_page;
	}
	phy_addr p_addr = va_to_pa((virt_addr)current);
	page_free(p_addr, 1);
}

/**
 * @brief Appends a block descriptor to the tail of the allocated list.
 *
 * @param block The descriptor to add. A NULL @p block is ignored.
 */
static void add_to_allocated_list(allocated_block_desc_t *block)
{
	if (block == NULL) {
		return;
	}

	block->next = NULL;

	if (allocated == NULL) {
		allocated = block;
		return;
	}

	allocated_block_desc_t *current = allocated;
	while (current->next != NULL) {
		current = current->next;
	}

	current->next = block;
}

/**
 * @brief Removes the descriptor for @p ptr from the allocated list.
 *
 * Unlinks the descriptor whose allocation starts at @p ptr and marks it free.
 *
 * @param ptr Virtual address returned by a previous kmalloc().
 * @return The removed descriptor, or NULL if @p ptr was not found.
 */
static allocated_block_desc_t *remove_from_allocated_list(void *ptr)
{
	if (ptr == NULL || allocated == NULL) {
		return NULL;
	}

	if (allocated->ptr == ptr) {
		allocated_block_desc_t *removed = allocated;
		allocated = allocated->next;

		removed->is_used = false;
		return removed;
	}

	allocated_block_desc_t *current = allocated;
	allocated_block_desc_t *prev = NULL;
	while (current != NULL) {
		if (current->ptr == ptr) {
			if (prev == NULL) {
				allocated = current->next;
			} else {
				prev->next = current->next;
			}
			current->is_used = false;
			return current;
		}
		prev = current;
		current = current->next;
	}
	return NULL;
}

void *kmalloc(size_t size)
{
	if (size == 0) {
		return NULL;
	}

	/*
	 * Guard the page-rounding below against size_t overflow: if size is
	 * within (PAGE_SIZE - 1) of SIZE_MAX, (size + PAGE_SIZE - 1) wraps and
	 * under-counts num_pages, which would under-allocate while block->size
	 * still records the full request.
	 */
	if (size > SIZE_MAX - (PAGE_SIZE - 1)) {
		return NULL;
	}

	allocated_block_desc_t *block = find_free_management_block();
	if (block == NULL) {
		kprintf("Failed to find free management block for kmalloc\n");
		return NULL;
	}

	/*
	 * NOTE: This is a naive implementation that rounds every request up to
	 * whole pages, so even a single-byte allocation consumes a full page.
	 * This wastes memory but keeps the allocator simple for now; it can be
	 * replaced with a sub-page/free-list scheme later without changing the
	 * kmalloc()/kfree() interface, which is what callers depend on.
	 */
	size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	phy_addr p_addr = page_alloc(num_pages);
	if (p_addr == PHYS_ADDR_NULL) {
		kprintf("Failed to allocate pages for kmalloc\n");
		block->is_used = false; // Release the management block
		return NULL;
	}

	// NOLINTNEXTLINE(*-int-to-ptr)
	block->ptr = (void *)pa_to_va(p_addr);
	block->size = size;
	add_to_allocated_list(block);

	return block->ptr;
}

void kfree(void *ptr)
{
	allocated_block_desc_t *block = remove_from_allocated_list(ptr);
	if (block != NULL) {
		phy_addr p_addr = va_to_pa((virt_addr)block->ptr);
		size_t num_pages = (block->size + PAGE_SIZE - 1) / PAGE_SIZE;
		page_free(p_addr, num_pages);
		free_management_page_if_empty(block);
	} else {
		kprintf("Attempted to free unallocated pointer: %p\n", ptr);
	}
}

#ifdef RUN_TESTS

void invalidate_kalloc_state(void)
{
	allocated = NULL;
	kalloc_page_head = NULL;
}

#endif /* RUN_TESTS */
