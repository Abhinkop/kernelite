/**
 * @file linker_symbol_test.c
 * @brief Test suite for verifying linker symbol layout and linker-defined
 * addresses.
 */

#include "../src/include/linker/linker_defines.h"
#include "../src/include/linker/symbols.h"

#include "test.h"

bool linker_symbols(void)
{
	uintptr_t image_start_addr = (uintptr_t)&image_start;
	uintptr_t image_end_addr = (uintptr_t)&image_end;
	uintptr_t idmap_pg_dir_start_addr = (uintptr_t)&idmap_pg_dir_start;
	uintptr_t kernel_pg_dir_root_start_addr =
		(uintptr_t)&kernel_pg_dir_root_start;

	EXPECT(image_start_addr < image_end_addr);

	EXPECT(idmap_pg_dir_start_addr < image_end_addr);
	EXPECT(idmap_pg_dir_start_addr > image_start_addr);
	EXPECT(idmap_pg_dir_start_addr + (uintptr_t)ID_MAP_SIZE <
	       image_end_addr);
	EXPECT(idmap_pg_dir_start_addr + (uintptr_t)ID_MAP_SIZE >
	       image_start_addr);
	EXPECT(idmap_pg_dir_start_addr + (uintptr_t)ID_MAP_SIZE ==
	       kernel_pg_dir_root_start_addr);

	EXPECT(kernel_pg_dir_root_start_addr < image_end_addr);
	EXPECT(kernel_pg_dir_root_start_addr > image_start_addr);
	EXPECT(kernel_pg_dir_root_start_addr + (uintptr_t)LINKER_PAGE_SIZE <
	       image_end_addr);
	EXPECT(kernel_pg_dir_root_start_addr + (uintptr_t)LINKER_PAGE_SIZE >
	       image_start_addr);

	return true;
}

/**
 * @brief Return the linker symbol test suite.
 *
 * @return A fully populated test suite containing the linker symbol regression
 *         test.
 */
test_suite_t get_linker_symbol_test_suite(void)
{
	test_suite_t suite = { .suite_name = "linker_symbols",
			       .tests = { { .test_name = "linker_symbols",
					    .test_fn = linker_symbols } },
			       .num_tests = 1 };
	return suite;
}
