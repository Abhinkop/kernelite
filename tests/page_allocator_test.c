/**
 * @file page_allocator_test.c
 * @brief Tests for the physical page allocator.
 */

#include "../src/include/allocator/page_allocator.h"
#include "../src/include/linker/linker_defines.h"
#include "../src/include/linker/symbols.h"
#include "../src/include/mem_layout/mem_layout.h"
#include "test.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Convenience: page size in bytes. */
#define PAGE_SZ ((size_t)LINKER_PAGE_SIZE)

/**
 * @brief Allocating a single page returns a non-NULL, page-aligned pointer.
 */
static bool test_alloc_single_page(void)
{
	void *ptr = page_alloc(1);

	EXPECT_NOT_NULL(ptr);
	/* Must be 4 KB aligned. */
	EXPECT_EQ((uintptr_t)ptr % PAGE_SZ, 0);
	return true;
}

/**
 * @brief Allocating multiple contiguous pages returns one aligned block.
 */
static bool test_alloc_multi_page(void)
{
	void *ptr = page_alloc(4);

	EXPECT_NOT_NULL(ptr);
	EXPECT_EQ((uintptr_t)ptr % PAGE_SZ, 0);
	return true;
}

/**
 * @brief Requesting zero pages must return NULL — not a valid allocation.
 */
static bool test_alloc_zero_pages_returns_null(void)
{
	void *ptr = page_alloc(0);

	EXPECT_NULL(ptr);
	return true;
}

/**
 * @brief Freeing a page and reallocating it must return the same address.
 */
static bool test_free_then_realloc_returns_same_page(void)
{
	void *first = page_alloc(1);
	EXPECT_NOT_NULL(first);

	page_free(va_to_pa((virt_addr)first), 1);

	void *second = page_alloc(1);
	EXPECT_NOT_NULL(second);
	EXPECT_EQ((uintptr_t)first, (uintptr_t)second);
	return true;
}

/**
 * @brief Two consecutive allocations must not overlap.
 */
static bool test_two_allocs_do_not_overlap(void)
{
	void *aaa = page_alloc(1);
	void *bbb = page_alloc(1);

	EXPECT_NOT_NULL(aaa);
	EXPECT_NOT_NULL(bbb);
	/* They must be at least PAGE_SZ apart. */
	uintptr_t diff = (uintptr_t)bbb > (uintptr_t)aaa ?
				 (uintptr_t)bbb - (uintptr_t)aaa :
				 (uintptr_t)aaa - (uintptr_t)bbb;
	EXPECT_GE(diff, PAGE_SZ);
	return true;
}

/**
 * @brief Allocating N pages, freeing them all, then allocating N pages again
 *        must succeed.
 */
static bool test_alloc_free_alloc_multi(void)
{
	const size_t num_pages = 8;

	void *ptr = page_alloc(num_pages);
	EXPECT_NOT_NULL(ptr);

	page_free(va_to_pa((virt_addr)ptr), num_pages);

	void *ptr_after = page_alloc(num_pages);
	EXPECT_NOT_NULL(ptr_after);
	EXPECT_EQ((uintptr_t)ptr, (uintptr_t)ptr_after);
	return true;
}

/**
 * @brief Reserving a page and then trying to allocate at that address must
 *        return a different page.
 */
static bool test_reserved_page_not_returned_by_alloc(void)
{
	/*
	 * Allocate one page to find out the base of the usable region,
	 * then free it and reserve it.  The next alloc must skip it.
	 */
	void *base = page_alloc(1);
	EXPECT_NOT_NULL(base);
	page_free(va_to_pa((virt_addr)base), 1);

	bool reserved = reserve_page(va_to_pa((virt_addr)base), 1);
	EXPECT(reserved);

	void *next = page_alloc(1);
	EXPECT_NOT_NULL(next);
	EXPECT_NEQ((uintptr_t)next, (uintptr_t)base);
	return true;
}

/**
 * @brief reserve_page with an address completely outside the managed range
 *        must return false.
 */
static bool test_reserve_out_of_bounds_fails(void)
{
	/*
	 * 0x0 is well below the RAM base on QEMU virt (0x40000000), so it
	 * is guaranteed to be out of the managed region.
	 */
	bool result = reserve_page(0x0UL, 1);

	EXPECT(!result);
	return true;
}

/**
 * @brief The page allocator bitmap pages must never be returned by page_alloc.
 */
static bool test_bitmap_pages_never_allocated(void)
{
	uintptr_t bitmap_start = (uintptr_t)&page_allocator_bit_map_start;
	uintptr_t bitmap_end = bitmap_start + LINKER_BITMAP_SIZE;

	void *pages[32];
	size_t index = 0;

	for (index = 0; index < 32; index++) {
		pages[index] = page_alloc(1);
		if (!pages[index])
			break;

		uintptr_t addr = (uintptr_t)pages[index];
		uintptr_t end = addr + PAGE_SZ;

		/* The page must not overlap the bitmap region at all. */
		bool overlaps = (addr < bitmap_end) && (end > bitmap_start);
		if (overlaps) {
			kprintf("FAIL: page_alloc returned 0x%lx which overlaps bitmap [0x%lx, 0x%lx) index",
				addr, bitmap_start, bitmap_end);
			return false;
		}
	}

	EXPECT(index > 0);
	return true;
}

test_suite_t get_page_allocator_test_suite(void)
{
	test_suite_t suite = {
		.suite_name = "page_allocator",
		.num_tests = 0,
	};

#define ADD(fn)                                       \
	suite.tests[suite.num_tests].test_name = #fn; \
	suite.tests[suite.num_tests].test_fn = fn;    \
	suite.num_tests++

	ADD(test_alloc_single_page);
	ADD(test_alloc_multi_page);
	ADD(test_alloc_zero_pages_returns_null);
	ADD(test_free_then_realloc_returns_same_page);
	ADD(test_two_allocs_do_not_overlap);
	ADD(test_alloc_free_alloc_multi);
	ADD(test_reserved_page_not_returned_by_alloc);
	ADD(test_reserve_out_of_bounds_fails);
	ADD(test_bitmap_pages_never_allocated);

#undef ADD
	return suite;
}
