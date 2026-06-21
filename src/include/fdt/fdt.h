/**
 * @file fdt.h
 * @brief Header that interfaces with libfdt for Device Tree Blob (DTB) parsing.
 *
 * Declares the kernel-facing FDT helper APIs used to validate and extract
 * memory mapping information from the device tree.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-16
 */

#ifndef FDT_FDT_H
#define FDT_FDT_H

#include "mem_layout/mem_layout.h"

#include <stdint.h>
#include <stdbool.h>

/** @brief Maximum number of distinct memory regions the kernel will track. */
#define MAX_MEM_REGIONS 16

/**
 * @brief Represents a single physical memory span.
 */
typedef struct {
	/** @brief Start physical address of the region. */
	phy_addr base;

	/** @brief Size of the region in bytes. */
	uint64_t size;
} Memory_region_t;

/**
 * @brief Container for the system physical memory layout.
 */
typedef struct {
	/** @brief Array of discovered regions. */
	Memory_region_t regions[MAX_MEM_REGIONS];

	/** @brief Actual number of regions populated. */
	int count;
} Memory_map_t;

/**
 * @brief Check the validity of the Device Tree Blob (FDT).
 * @param ptr Pointer to the FDT in memory.
 * @return true if the FDT is valid, false otherwise.
 */
bool check_fdt(const void *ptr);

/**
 * @brief Scans the FDT for memory nodes and populates the provided map.
 *
 * This function searches the FDT for nodes with `device_type = "memory"`,
 * respects the root node's `#address-cells` and `#size-cells`, and
 * handles both 32-bit and 64-bit address formats.
 *
 * @param fdt Pointer to the FDT header in memory.
 * @param mmap Pointer to a Memory_map_t structure to be populated.
 * @return 0 on success, or a negative libfdt error code on failure.
 */
int get_mem(const void *fdt, Memory_map_t *mmap);

/**
 * @brief Retrieves a pointer to the interrupt controller node in the FDT.
 * @param fdt Pointer to the FDT header in memory.
 * @return Offset of the interrupt controller node, or a negative libfdt error
 * code if not found.
 */
int get_intc_node_offset(const void *fdt);

/**
 * @brief Checks if the given FDT offset represents an error.
 * @param offset The FDT offset to check.
 * @return true if the offset is an error code, false otherwise.
 */
bool fdt_is_error(int offset);

/**
 * @brief Retrieves the compatible string for a given node.
 * @param fdt Pointer to the FDT header in memory.
 * @param node_offset Offset of the node for which to retrieve the compatible
 * string.
 * @return Pointer to the compatible string, or NULL if not found.
 */
char *get_compatible_string(const void *fdt, int node_offset);

/**
 * @brief Retrieves the register property for a given node.
 * @param fdt Pointer to the FDT header in memory.
 * @param node_offset Offset of the node for which to retrieve the register
 * property.
 * @param reg_map Pointer to a Memory_map_t structure to be populated with
 * register information.
 * @return true if the register property is found and successfully parsed, false
 * otherwise.
 */
bool get_reg_property(const void *fdt, int node_offset, Memory_map_t *reg_map);

#endif // FDT_FDT_H
