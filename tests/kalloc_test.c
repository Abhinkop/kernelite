/**
 * @file kalloc_test.c
 * @brief Tests for the kernel heap allocator (kmalloc/kfree).
 *
 * These tests run before the MMU is enabled, so pa_to_va()/va_to_pa() are
 * identity mappings and the pointers kmalloc() returns address usable
 * physical memory directly.
 *
 * Each test builds its own single physical region from scratch:
 * invalidate_all_memory_chunks() drops the boot region the runner installed,
 * page_allocator_add_region() registers a known region, and
 * invalidate_kalloc_state() clears the heap allocator's bookkeeping so no
 * management page or allocation record leaks in from the previous test. This
 * mirrors the multiregion page_allocator tests and makes every returned
 * address deterministic.
 *
 * Layout of the test region: page 0 holds the allocator bitmap, so the first
 * usable page is REGION_BASE + PAGE_SZ. The first kmalloc() lazily grabs one
 * page for its management pool (REGION_BASE + 1*PAGE_SZ), after which data
 * allocations begin at REGION_BASE + 2*PAGE_SZ.
 */

#include "../src/include/allocator/kalloc.h"
#include "../src/include/allocator/page_allocator.h"
#include "../src/include/linker/linker_defines.h"

#include "test.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Convenience: page size in bytes. */
#define PAGE_SZ ((size_t)LINKER_PAGE_SIZE)

/** @brief Physical base of the test region (real RAM, above the kernel image).
 */
#define REGION_BASE ((phy_addr)0x47000000)

/** @brief Size of the test region: 16 pages (1 bitmap + 15 usable). */
#define REGION_SIZE ((size_t)0x10000)

/** @brief Address of the first data allocation (page 0 bitmap, page 1 mgmt). */
#define FIRST_DATA_ADDR (REGION_BASE + 2 * PAGE_SZ)

/*
 * kmalloc()/kfree() are a custom physical allocator, not libc malloc/free. The
 * static analyzer's malloc model does not apply — it flags kfree() of the
 * page-backed addresses these tests use — so scope it out for this file.
 */
// NOLINTBEGIN(clang-analyzer-unix.Malloc)

/**
 * @brief Resets both allocators and installs a single known region.
 *
 * Every test calls this first so it starts from an empty page pool and empty
 * heap bookkeeping with deterministic addresses.
 */
static void setup_region(void)
{
	invalidate_all_memory_chunks();
	invalidate_kalloc_state();
	page_allocator_add_region(REGION_BASE, REGION_SIZE, false);
}

/**
 * @brief A basic allocation returns the first data page, non-null and aligned.
 */
static bool test_kmalloc_returns_first_data_page(void)
{
	setup_region();

	void *ptr = kmalloc(64);

	EXPECT_NOT_NULL(ptr);
	EXPECT_EQ((uintptr_t)ptr % PAGE_SZ, 0);
	EXPECT_EQ((uintptr_t)ptr, (uintptr_t)FIRST_DATA_ADDR);
	return true;
}

/**
 * @brief Requesting zero bytes must return NULL.
 */
static bool test_kmalloc_zero_returns_null(void)
{
	setup_region();

	EXPECT_NULL(kmalloc(0));
	return true;
}

/**
 * @brief Two live allocations occupy consecutive data pages and do not alias.
 */
static bool test_kmalloc_distinct_allocations(void)
{
	setup_region();

	void *first = kmalloc(64);
	void *second = kmalloc(64);

	EXPECT_EQ((uintptr_t)first, (uintptr_t)FIRST_DATA_ADDR);

	// TODO(kalloc-optimization)
	// This is because a new allocation grabs the next page after the first
	// allocation, which is not optimal. This would be removed when we
	// optimize the allocations
	EXPECT_EQ((uintptr_t)second, (uintptr_t)(FIRST_DATA_ADDR + PAGE_SZ));
	EXPECT_NEQ(first, second);
	return true;
}

/**
 * @brief The whole returned block is writable and reads back what was stored.
 */
static bool test_kmalloc_block_is_writable(void)
{
	setup_region();

	const size_t size = 128;
	uint8_t *bytes = (uint8_t *)kmalloc(size);
	EXPECT_NOT_NULL(bytes);

	for (size_t i = 0; i < size; i++) {
		bytes[i] = (uint8_t)i;
	}
	for (size_t i = 0; i < size; i++) {
		EXPECT_EQ(bytes[i], (uint8_t)i);
	}
	return true;
}

/**
 * @brief An allocation larger than one page spans contiguous, writable pages.
 */
static bool test_kmalloc_multipage_is_writable(void)
{
	setup_region();

	uint8_t *bytes = (uint8_t *)kmalloc(PAGE_SZ + 1);
	EXPECT_NOT_NULL(bytes);
	EXPECT_EQ((uintptr_t)bytes, (uintptr_t)FIRST_DATA_ADDR);

	/* Touch the first byte, a byte in the second page, and read them back.
	 */
	bytes[0] = 0x11;
	bytes[PAGE_SZ] = 0x22;
	EXPECT_EQ(bytes[0], 0x11);
	EXPECT_EQ(bytes[PAGE_SZ], 0x22);
	return true;
}

/**
 * @brief Freeing an allocation returns its pages so the next request reuses
 *        the same address.
 */
static bool test_kfree_then_kmalloc_reuses_page(void)
{
	setup_region();

	void *first = kmalloc(64);
	EXPECT_EQ((uintptr_t)first, (uintptr_t)FIRST_DATA_ADDR);

	kfree(first);

	void *second = kmalloc(64);
	EXPECT_EQ((uintptr_t)second, (uintptr_t)FIRST_DATA_ADDR);
	EXPECT_EQ(first, second);
	return true;
}

/**
 * @brief Freeing NULL is a harmless no-op.
 */
static bool test_kfree_null_is_safe(void)
{
	setup_region();

	kfree(NULL);
	/* Reaching here without faulting is the assertion. */
	return true;
}

/**
 * @brief After freeing the first block, a new allocation reuses its page and
 *        does not collide with the still-live second block.
 */
static bool test_kmalloc_after_free_no_collision(void)
{
	setup_region();

	void *first = kmalloc(64);
	void *second = kmalloc(64);
	EXPECT_EQ((uintptr_t)first, (uintptr_t)FIRST_DATA_ADDR);
	EXPECT_EQ((uintptr_t)second, (uintptr_t)(FIRST_DATA_ADDR + PAGE_SZ));

	kfree(first);

	void *third = kmalloc(64);
	/* third reclaims first's freed page and must not alias second. */
	EXPECT_EQ((uintptr_t)third, (uintptr_t)FIRST_DATA_ADDR);
	EXPECT_NEQ(third, second);
	return true;
}

/**
 * @brief Freeing the last allocation in a management page returns that page to
 *        the page allocator (no management-page leak).
 */
static bool test_kfree_reclaims_management_page(void)
{
	setup_region();

	/*
	 * The first kmalloc() grabs the management page at REGION_BASE +
	 * PAGE_SZ and its data page at FIRST_DATA_ADDR (REGION_BASE +
	 * 2*PAGE_SZ).
	 */
	void *ptr = kmalloc(64);
	EXPECT_EQ((uintptr_t)ptr, (uintptr_t)FIRST_DATA_ADDR);

	kfree(ptr);

	/*
	 * kfree() returns the data page and, since its management page is now
	 * empty, that page too. Both are free and contiguous starting at
	 * REGION_BASE + PAGE_SZ, so a direct 2-page allocation must reclaim
	 * from there. If the management page had leaked, REGION_BASE + PAGE_SZ
	 * would still be reserved and this allocation would land elsewhere.
	 */
	phy_addr block = page_alloc(2);
	EXPECT_EQ(block, REGION_BASE + PAGE_SZ);
	return true;
}

/**
 * @brief A size that would overflow the page-rounding is rejected, not
 *        under-allocated.
 */
static bool test_kmalloc_overflow_size_returns_null(void)
{
	setup_region();

	/*
	 * (size + PAGE_SZ - 1) wraps for sizes within (PAGE_SZ - 1) of
	 * SIZE_MAX. kmalloc() must reject these up front rather than round down
	 * to a too-small page count.
	 */
	EXPECT_NULL(kmalloc(SIZE_MAX));
	EXPECT_NULL(kmalloc(SIZE_MAX - (PAGE_SZ - 1) + 1));
	EXPECT_NOT_NULL(kmalloc(REGION_SIZE - 2 * PAGE_SZ));

	return true;
}

test_suite_t get_kalloc_test_suite(void)
{
	test_suite_t suite = {
		.suite_name = "kalloc",
		.num_tests = 0,
	};

#define ADD(fn)                                       \
	suite.tests[suite.num_tests].test_name = #fn; \
	suite.tests[suite.num_tests].test_fn = fn;    \
	suite.num_tests++

	ADD(test_kmalloc_returns_first_data_page);
	ADD(test_kmalloc_zero_returns_null);
	ADD(test_kmalloc_distinct_allocations);
	ADD(test_kmalloc_block_is_writable);
	ADD(test_kmalloc_multipage_is_writable);
	ADD(test_kfree_then_kmalloc_reuses_page);
	ADD(test_kfree_null_is_safe);
	ADD(test_kmalloc_overflow_size_returns_null);
	ADD(test_kmalloc_after_free_no_collision);
	ADD(test_kfree_reclaims_management_page);

#undef ADD
	return suite;
}

// NOLINTEND(clang-analyzer-unix.Malloc)
