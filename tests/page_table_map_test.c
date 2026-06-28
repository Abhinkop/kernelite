/**
 * @file page_table_map_test.c
 * @brief Tests for page table mapping functionality.
 *
 * Validates that virtual addresses are correctly mapped to physical addresses
 * with the appropriate permissions and that the resulting page table
 * structures are consistent.
 */

#include "../src/include/page_table/page_table.h"
#include "../src/include/allocator/page_allocator.h"
#include "../src/include/linker/symbols.h"
#include "../src/include/linker/linker_defines.h"

#include "test.h"

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Retrieve a next-level page table pointer from a table descriptor.
 *
 * This test helper reassembles the physical address stored in a table
 * descriptor and returns the corresponding virtual pointer.
 *
 * @param entry Pointer to the table descriptor.
 * @return Pointer to the next-level page table, or NULL if the entry is
 * invalid.
 */
static inline page_table_t *
get_page_table_entry_address(const table_desc_t *entry)
{
	// Return a clean 0 if the entry is unmapped.
	// Note: Upstream logic should always check entry->valid before calling
	// this.
	if (!entry->valid) {
		return NULL;
	}

	phy_addr phys_addr = 0;

	// Stitch the non-contiguous physical address windows back together
	phys_addr |= ((phy_addr)entry->nlta_47_12) << 12;
	phys_addr |= ((phy_addr)entry->nlta_49_48) << 48;
	phys_addr |= ((phy_addr)entry->nlta_51_50) << 50;

	// NOLINTNEXTLINE(*-int-to-ptr)
	return (page_table_t *)phys_addr;
}

/**
 * @brief Retrieve a physical frame address from a page descriptor.
 *
 * Reconstructs the fragmented page frame address from the descriptor fields.
 *
 * @param entry Pointer to the page descriptor.
 * @return Physical frame address, or 0 if the descriptor is invalid.
 */
static inline phy_addr get_page_desc_entry_address(const page_desc_t *entry)
{
	// Return a clean numeric 0 if the mapping is invalid
	if (!entry->valid) {
		return 0;
	}

	phy_addr phys_addr = 0;

	// Stitch the fragmented Output Address (OA) frames back together
	phys_addr |= ((phy_addr)entry->frame_addr_47_12) << 12;
	phys_addr |= ((phy_addr)entry->frame_addr_49_48) << 48;

	return phys_addr;
}

/**
 * @brief Verify that a virtual address maps to the expected physical frame.
 *
 * Walks the four-level page table hierarchy and verifies the resulting L3
 * page descriptor points to @p expected_pa.
 *
 * @param root        Root of the page table hierarchy.
 * @param v_addr      Virtual address being verified.
 * @param expected_pa Expected physical address for the mapping.
 * @return true when the mapping is correct.
 */
bool verify_map(page_table_t *root, virt_addr v_addr, phy_addr expected_pa)
{
	// Get Indices
	const uint64_t l0_idx_start_bit = 39;
	const uint64_t l1_idx_start_bit = 30;
	const uint64_t l2_idx_start_bit = 21;
	const uint64_t l3_idx_start_bit = 12;
	const uint64_t idx_mask = 0x1FFUL; // 9 bits for each level index

	const uint64_t l0_idx = (v_addr >> l0_idx_start_bit) & idx_mask;
	const uint64_t l1_idx = (v_addr >> l1_idx_start_bit) & idx_mask;
	const uint64_t l2_idx = (v_addr >> l2_idx_start_bit) & idx_mask;
	const uint64_t l3_idx = (v_addr >> l3_idx_start_bit) & idx_mask;

	EXPECT(root->entries[l0_idx].value);
	page_table_t *level1 =
		get_page_table_entry_address(&root->entries[l0_idx].table_desc);
	EXPECT(level1 != NULL);
	EXPECT(level1->entries[l1_idx].value);
	page_table_t *level2 = get_page_table_entry_address(
		&level1->entries[l1_idx].table_desc);
	EXPECT(level2 != NULL);
	EXPECT(level2->entries[l2_idx].value);
	page_table_t *level3 = get_page_table_entry_address(
		&level2->entries[l2_idx].table_desc);
	EXPECT(level3 != NULL);
	phy_addr actual_pa =
		get_page_desc_entry_address(&level3->entries[l3_idx].page_desc);
	kprintf("actual = %lx, expected %lx\n", actual_pa, expected_pa);
	EXPECT(actual_pa == expected_pa);

	// Todo: CHeck validity of permisions also here.
	return true;
}

bool test_page_table_mapping(page_table_t *root, virt_addr v_addr,
			     phy_addr phy_addr, page_permissions_t test_perms)
{
	EXPECT(root != NULL);
	EXPECT(map_page(root, v_addr, phy_addr, test_perms, NORMAL));
	EXPECT(verify_map(root, v_addr, phy_addr));

	return true; // Indicate test passed
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
/**
 * @brief Run the page table mapping regression tests.
 *
 * Verifies the kernel page table implementation can allocate intermediate
 * tables and install valid L3 page mappings for a range of virtual address
 * regions.
 *
 * @return true when all test vectors pass.
 */
bool page_table_map(void)
{
	page_table_t *root = (page_table_t *)page_alloc(1);

	// Group 1: Share L0, L1, L2 — only L3 entries differ
	// VA bits [47:39]=0, [38:30]=0, [29:21]=0, [20:12]=1,2,3,4
	EXPECT(test_page_table_mapping(root, 0xFFFFF00000001000, 0x1000,
				       (page_permissions_t){ .read = 1,
							     .write = 0,
							     .execute = 0,
							     .user_accessible =
								     0 }));
	EXPECT(test_page_table_mapping(root, 0xFFFFF00000002000, 0x2000,
				       (page_permissions_t){ .read = 1,
							     .write = 1,
							     .execute = 0,
							     .user_accessible =
								     0 }));
	EXPECT(test_page_table_mapping(root, 0xFFFFF00000003000, 0x3000,
				       (page_permissions_t){ .read = 1,
							     .write = 0,
							     .execute = 1,
							     .user_accessible =
								     0 }));
	EXPECT(test_page_table_mapping(root, 0xFFFFF00000004000, 0x4000,
				       (page_permissions_t){ .read = 1,
							     .write = 1,
							     .execute = 1,
							     .user_accessible =
								     0 }));

	// Group 2: Different L1 region — forces new L2 table allocation
	// VA bits [47:39]=0, [38:30]=1,2,3,4, [29:21]=0, [20:12]=0
	EXPECT(test_page_table_mapping(root, 0xFFFFF00040000000, 0x5000,
				       (page_permissions_t){ .read = 1,
							     .write = 0,
							     .execute = 1,
							     .user_accessible =
								     0 }));
	EXPECT(test_page_table_mapping(root, 0xFFFFF00080000000, 0x6000,
				       (page_permissions_t){ .read = 1,
							     .write = 0,
							     .execute = 0,
							     .user_accessible =
								     0 }));
	EXPECT(test_page_table_mapping(root, 0xFFFFF000C0000000, 0x7000,
				       (page_permissions_t){ .read = 1,
							     .write = 1,
							     .execute = 0,
							     .user_accessible =
								     0 }));
	EXPECT(test_page_table_mapping(root, 0xFFFFF00100000000, 0x8000,
				       (page_permissions_t){ .read = 1,
							     .write = 1,
							     .execute = 0,
							     .user_accessible =
								     0 }));

	// Group 3: Different L0 region — forces new L1 table allocation
	// VA bits [47:39]=1,2,3,4, [38:30]=0, [29:21]=0, [20:12]=0
	EXPECT(test_page_table_mapping(root, 0xFFFFF08000000000, 0x9000,
				       (page_permissions_t){ .read = 1,
							     .write = 0,
							     .execute = 1,
							     .user_accessible =
								     0 }));
	EXPECT(test_page_table_mapping(root, 0xFFFFF10000000000, 0xA000,
				       (page_permissions_t){ .read = 1,
							     .write = 0,
							     .execute = 1,
							     .user_accessible =
								     0 }));
	EXPECT(test_page_table_mapping(root, 0xFFFFF18000000000, 0xB000,
				       (page_permissions_t){ .read = 1,
							     .write = 1,
							     .execute = 1,
							     .user_accessible =
								     0 }));
	EXPECT(test_page_table_mapping(root, 0xFFFFF20000000000, 0xC000,
				       (page_permissions_t){ .read = 1,
							     .write = 1,
							     .execute = 1,
							     .user_accessible =
								     0 }));
	return true;
}

/**
 * @brief Verify the initial identity page table map can be created.
 *
 * Ensures the kernel can initialize the static identity-map root pages and
 * set up a simple device mapping for the UART base address.
 *
 * @return true when the identity map initialization succeeds.
 */
bool init_page_table_map(void)
{
	virt_addr uart0_base = 0x09000000;
	EXPECT(setup_kernel_id_map());
	EXPECT(map_page(get_id_map_root(), uart0_base, uart0_base,
			(page_permissions_t){
				.execute = false, .read = true, .write = true },
			DEVICE))
	return true;
}

/**
 * @brief Return the page table test suite.
 *
 * Populates the suite structure with page table test cases for the test
 * runner.
 *
 * @return A fully initialized test suite containing page table tests.
 */
test_suite_t get_page_table_test_suite(void)
{
	_Static_assert(sizeof(page_table_entry_t) == 8,
		       "FATAL: ARM64 PTE size must equal exactly 8 bytes!");
	test_suite_t suite = { .suite_name = "page_table",
			       .tests = { { .test_name = "page_table_map",
					    .test_fn = page_table_map },
					  { .test_name = "init_page_table_map",
					    .test_fn = init_page_table_map } },
			       .num_tests = 2 };
	return suite;
}
