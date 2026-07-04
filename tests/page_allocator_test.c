/**
 * @file page_allocator_test.c
 * @brief Tests for the physical page allocator.
 */

#include "../src/include/allocator/page_allocator.h"
#include "../src/include/fdt/fdt.h"
#include "../src/include/linker/linker_defines.h"
#include "../src/include/linker/symbols.h"
#include "../src/include/mem_layout/mem_layout.h"
#include "test.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Convenience: page size in bytes. */
#define PAGE_SZ ((size_t)LINKER_PAGE_SIZE)

/*
 * Allocator reset between tests
 *
 * The runner calls setup_page_allocator() before every test, which rebuilds
 * the boot region, so the standard single-region tests below start from a
 * clean allocator without doing anything themselves. The multiregion tests
 * further down instead call invalidate_all_memory_chunks() up front: they
 * register their own regions and must first drop the boot region that
 * setup_page_allocator() just added. See the comment above those tests for
 * details.
 */

/**
 * @brief Allocating a single page returns a non-null, page-aligned physical
 *        address.
 */
static bool test_alloc_single_page(void)
{
	phy_addr ptr = page_alloc(1);

	EXPECT_NEQ(ptr, PHYS_ADDR_NULL);
	/* Must be 4 KB aligned. */
	EXPECT_EQ((uintptr_t)ptr % PAGE_SZ, 0);
	return true;
}

/**
 * @brief Allocating multiple contiguous pages returns one aligned block.
 */
static bool test_alloc_multi_page(void)
{
	phy_addr ptr = page_alloc(4);

	EXPECT_NEQ(ptr, PHYS_ADDR_NULL);
	EXPECT_EQ((uintptr_t)ptr % PAGE_SZ, 0);
	return true;
}

/**
 * @brief Requesting zero pages must return PHYS_ADDR_NULL — not a valid
 *        allocation.
 */
static bool test_alloc_zero_pages_returns_null(void)
{
	phy_addr ptr = page_alloc(0);

	EXPECT_EQ(ptr, PHYS_ADDR_NULL);
	return true;
}

/**
 * @brief Freeing a page and reallocating it must return the same address.
 */
static bool test_free_then_realloc_returns_same_page(void)
{
	phy_addr first = page_alloc(1);
	EXPECT_NEQ(first, PHYS_ADDR_NULL);

	page_free(first, 1);

	phy_addr second = page_alloc(1);
	EXPECT_NEQ(second, PHYS_ADDR_NULL);
	EXPECT_EQ((uintptr_t)first, (uintptr_t)second);
	return true;
}

/**
 * @brief Two consecutive allocations must not overlap.
 */
static bool test_two_allocs_do_not_overlap(void)
{
	phy_addr aaa = page_alloc(1);
	phy_addr bbb = page_alloc(1);

	EXPECT_NEQ(aaa, PHYS_ADDR_NULL);
	EXPECT_NEQ(bbb, PHYS_ADDR_NULL);
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

	phy_addr ptr = page_alloc(num_pages);
	EXPECT_NEQ(ptr, PHYS_ADDR_NULL);

	page_free(ptr, num_pages);

	phy_addr ptr_after = page_alloc(num_pages);
	EXPECT_NEQ(ptr_after, PHYS_ADDR_NULL);
	EXPECT_EQ(ptr, ptr_after);
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
	phy_addr base = page_alloc(1);
	EXPECT_NEQ(base, PHYS_ADDR_NULL);
	page_free(base, 1);

	bool reserved = reserve_page(base, 1);
	EXPECT(reserved);

	phy_addr next = page_alloc(1);
	EXPECT_NEQ(next, PHYS_ADDR_NULL);
	EXPECT_NEQ(next, base);
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
	phy_addr pages[32];
	size_t index = 0;

	for (index = 0; index < 32; index++) {
		pages[index] = page_alloc(1);
		if (!pages[index])
			break;

		uintptr_t addr = (uintptr_t)pages[index];
		uintptr_t end = addr + PAGE_SZ;

		size_t bitmap_size = 0;
		phy_addr bitmap_start = 0;

		get_bitmap_addr_and_size(pages[index], &bitmap_start,
					 &bitmap_size);
		phy_addr bitmap_end = bitmap_start + bitmap_size;

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

/**
 * @brief The page allocator bitmap pages must never be returned by page_alloc.
 */
static bool sample_multiregion_test(void)
{
	invalidate_all_memory_chunks();
	memory_region_t regions[2] = {
		{ .base = 0x47000000, .size = 0x5000 },
		{ .base = 0x47005000, .size = 0x5000 },
	};
	page_allocator_add_region(regions[0].base, regions[0].size, false);
	phy_addr ptr = page_alloc(1);
	EXPECT_EQ(ptr, regions[0].base + PAGE_SZ);
	ptr = page_alloc(1);
	EXPECT_EQ(ptr, regions[0].base + 2 * PAGE_SZ);
	ptr = page_alloc(1);
	EXPECT_EQ(ptr, regions[0].base + 3 * PAGE_SZ);
	ptr = page_alloc(1);
	EXPECT_EQ(ptr, regions[0].base + 4 * PAGE_SZ);
	page_allocator_add_region(regions[1].base, regions[1].size, false);
	ptr = page_alloc(1);
	EXPECT_EQ(ptr, regions[1].base + PAGE_SZ);
	return true;
}

/*
 * Multiregion tests
 *
 * Each registers explicit physical regions (real RAM well above the kernel
 * image on QEMU virt) and asserts the exact address every call returns. A
 * region's first page holds its bitmap, so the first usable page is base +
 * PAGE_SZ. invalidate_all_memory_chunks() at the top gives each test a known
 * empty allocator; the harness re-adds the boot region before the next test.
 */

/**
 * @brief Allocations fill the first region in order, then spill to the next,
 *        and fail once every region is exhausted.
 */
static bool test_alloc_fills_then_spills_then_exhausts(void)
{
	invalidate_all_memory_chunks();
	memory_region_t regions[2] = {
		{ .base = 0x47000000, .size = 0x5000 }, /* 4 usable pages */
		{ .base = 0x47005000, .size = 0x5000 }, /* 4 usable pages */
	};
	page_allocator_add_region(regions[0].base, regions[0].size, false);
	page_allocator_add_region(regions[1].base, regions[1].size, false);

	EXPECT_EQ(page_alloc(1), regions[0].base + 1 * PAGE_SZ);
	EXPECT_EQ(page_alloc(1), regions[0].base + 2 * PAGE_SZ);
	EXPECT_EQ(page_alloc(1), regions[0].base + 3 * PAGE_SZ);
	EXPECT_EQ(page_alloc(1), regions[0].base + 4 * PAGE_SZ);

	/* Region 0 is full, so allocations move to region 1. */
	EXPECT_EQ(page_alloc(1), regions[1].base + 1 * PAGE_SZ);
	EXPECT_EQ(page_alloc(1), regions[1].base + 2 * PAGE_SZ);
	EXPECT_EQ(page_alloc(1), regions[1].base + 3 * PAGE_SZ);
	EXPECT_EQ(page_alloc(1), regions[1].base + 4 * PAGE_SZ);

	/* Both regions exhausted. */
	EXPECT_EQ(page_alloc(1), PHYS_ADDR_NULL);
	return true;
}

/**
 * @brief Multi-page allocations return a contiguous block and pack tightly.
 */
static bool test_multi_page_alloc_is_contiguous(void)
{
	invalidate_all_memory_chunks();
	/* Region with 5 usable pages (page 0 holds the bitmap). */
	memory_region_t region = { .base = 0x47000000, .size = 0x6000 };
	page_allocator_add_region(region.base, region.size, false);

	/* A 3-page block starts at the first usable page... */
	EXPECT_EQ(page_alloc(3), region.base + 1 * PAGE_SZ);
	/* ...and the next 2-page block sits right after it. */
	EXPECT_EQ(page_alloc(2), region.base + 4 * PAGE_SZ);
	/* All 5 usable pages are gone. */
	EXPECT_EQ(page_alloc(1), PHYS_ADDR_NULL);
	return true;
}

/**
 * @brief An allocation too big for the first region is served by a later one.
 */
static bool test_alloc_skips_region_that_is_too_small(void)
{
	invalidate_all_memory_chunks();
	memory_region_t regions[2] = {
		{ .base = 0x47000000, .size = 0x3000 }, /* 2 usable pages */
		{ .base = 0x47010000, .size = 0x6000 }, /* 5 usable pages */
	};
	page_allocator_add_region(regions[0].base, regions[0].size, false);
	page_allocator_add_region(regions[1].base, regions[1].size, false);

	/* 4 pages don't fit in region 0, so region 1 serves the request. */
	EXPECT_EQ(page_alloc(4), regions[1].base + 1 * PAGE_SZ);
	return true;
}

/**
 * @brief An allocation larger than any single region fails; regions do not
 *        combine to satisfy one request.
 */
static bool test_alloc_does_not_span_regions(void)
{
	invalidate_all_memory_chunks();
	memory_region_t regions[2] = {
		{ .base = 0x47000000, .size = 0x5000 }, /* 4 usable pages */
		{ .base = 0x47005000, .size = 0x5000 }, /* 4 usable pages */
	};
	page_allocator_add_region(regions[0].base, regions[0].size, false);
	page_allocator_add_region(regions[1].base, regions[1].size, false);

	/* 5 contiguous pages fit in neither region. */
	EXPECT_EQ(page_alloc(5), PHYS_ADDR_NULL);
	return true;
}

/**
 * @brief A freed page is the next one handed back out.
 */
static bool test_freed_page_is_reused_first(void)
{
	invalidate_all_memory_chunks();
	/* Region with 4 usable pages. */
	memory_region_t region = { .base = 0x47000000, .size = 0x5000 };
	page_allocator_add_region(region.base, region.size, false);

	EXPECT_EQ(page_alloc(1), region.base + 1 * PAGE_SZ);
	phy_addr second = page_alloc(1);
	EXPECT_EQ(second, region.base + 2 * PAGE_SZ);
	EXPECT_EQ(page_alloc(1), region.base + 3 * PAGE_SZ);

	page_free(second, 1);
	/* Page 2 is the lowest free index again, so it comes back next. */
	EXPECT_EQ(page_alloc(1), region.base + 2 * PAGE_SZ);
	return true;
}

/**
 * @brief A reserved page is skipped by subsequent allocations.
 */
static bool test_reserved_page_is_skipped(void)
{
	invalidate_all_memory_chunks();
	/* Region with usable pages 1..4 (page 0 holds the bitmap). */
	memory_region_t region = { .base = 0x47000000, .size = 0x5000 };
	page_allocator_add_region(region.base, region.size, false);

	EXPECT(reserve_page(region.base + 2 * PAGE_SZ, 1));

	EXPECT_EQ(page_alloc(1), region.base + 1 * PAGE_SZ);
	/* Page 2 is reserved, so allocation jumps past it. */
	EXPECT_EQ(page_alloc(1), region.base + 3 * PAGE_SZ);
	EXPECT_EQ(page_alloc(1), region.base + 4 * PAGE_SZ);
	EXPECT_EQ(page_alloc(1), PHYS_ADDR_NULL);
	return true;
}

/**
 * @brief Removing a region excludes it from future allocations.
 */
static bool test_removed_region_is_not_allocated_from(void)
{
	invalidate_all_memory_chunks();
	memory_region_t regions[2] = {
		{ .base = 0x47000000, .size = 0x5000 },
		{ .base = 0x47005000, .size = 0x5000 },
	};
	page_allocator_add_region(regions[0].base, regions[0].size, false);
	page_allocator_add_region(regions[1].base, regions[1].size, false);

	EXPECT(page_allocator_remove_region(regions[0].base));

	/* Region 0 is gone, so region 1 serves the allocation. */
	EXPECT_EQ(page_alloc(1), regions[1].base + 1 * PAGE_SZ);
	return true;
}

/**
 * @brief Registering more than MAX_MEM_CHUNKS (10) regions is refused.
 */
static bool test_add_region_hits_chunk_limit(void)
{
	invalidate_all_memory_chunks();
	const phy_addr base = 0x47000000;
	const size_t sub = 0x2000; /* 1 bitmap + 1 usable page each */

	/* The first ten regions are accepted... */
	for (size_t i = 0; i < 10; i++) {
		EXPECT(page_allocator_add_region(base + i * sub, sub, false));
	}
	/* ...and the eleventh is refused rather than overflowing the table. */
	EXPECT(!page_allocator_add_region(base + 10 * sub, sub, false));
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
	ADD(sample_multiregion_test);
	ADD(test_alloc_fills_then_spills_then_exhausts);
	ADD(test_multi_page_alloc_is_contiguous);
	ADD(test_alloc_skips_region_that_is_too_small);
	ADD(test_alloc_does_not_span_regions);
	ADD(test_freed_page_is_reused_first);
	ADD(test_reserved_page_is_skipped);
	ADD(test_removed_region_is_not_allocated_from);
	ADD(test_add_region_hits_chunk_limit);

#undef ADD
	return suite;
}
