/**
 * @file kalloc.h
 * @brief Kernel heap allocator for variable-sized dynamic memory.
 *
 * This module provides the kernel-side dynamic memory API. It hands out
 * variable-sized allocations from the kernel heap and returns them for reuse,
 * layering on top of the physical page allocator.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-07-05
 */

#ifndef ALLOCATOR_KALLOC_H
#define ALLOCATOR_KALLOC_H

#include <stddef.h>

/**
 * @brief Allocates a block of memory from the kernel heap.
 *
 * Reserves at least @p size bytes of contiguous memory for the caller.
 *
 * @param size The number of bytes to allocate.
 * @return void* Pointer to the allocated block, or NULL if the request could
 * not be satisfied.
 */
void *kmalloc(size_t size);

/**
 * @brief Frees a block of memory previously returned by kmalloc().
 *
 * Returns the block starting at @p ptr to the kernel heap so it can be reused.
 * Passing NULL is a no-op.
 *
 * @param ptr Pointer to the block to free, as returned by kmalloc().
 */
void kfree(void *ptr);

#ifdef RUN_TESTS

/**
 * @brief Resets the heap allocator's bookkeeping to an empty state.
 *
 * Test-only helper that drops all management pages and the live-allocation
 * list so each test starts from a clean allocator. It does not free the
 * underlying physical pages; callers rely on the page allocator being reset
 * separately.
 */
void invalidate_kalloc_state(void);

#endif /* RUN_TESTS */

#endif /* ALLOCATOR_KALLOC_H */
