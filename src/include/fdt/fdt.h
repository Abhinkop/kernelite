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
#include "utils/kprintf.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** @brief Maximum number of distinct memory regions the kernel will track. */
#define MAX_MEM_REGIONS 16

/**
 * @brief Represents a single physical memory span.
 */
typedef struct memory_region_t {
	/** @brief Start physical address of the region. */
	phy_addr base;

	/** @brief Size of the region in bytes. */
	uint64_t size;
} memory_region_t;

/**
 * @brief Container for the system physical memory layout.
 */
typedef struct memory_map_t {
	/** @brief Array of discovered regions. */
	memory_region_t regions[MAX_MEM_REGIONS];

	/** @brief Actual number of regions populated. */
	int count;
} memory_map_t;

/**
 * @brief Interrupt category, as encoded in the first cell of an FDT
 * "interrupts" property.
 */
typedef enum {
	/** @brief Shared Peripheral Interrupt (routable to any core). */
	IRQ_TYPE_SPI = 0x0,

	/** @brief Private Peripheral Interrupt (local to a single core). */
	IRQ_TYPE_PPI = 0x1,
} irq_type_t;

/**
 * @brief Signal shape used to trigger an interrupt.
 */
typedef enum {
	/** @brief Interrupt is asserted on a signal edge. */
	IRQ_TRIGGER_EDGE,

	/** @brief Interrupt is asserted while the signal level is high. */
	IRQ_TRIGGER_LEVEL_HIGH,
} irq_trigger_t;

/**
 * @brief Describes a single interrupt as parsed from an FDT "interrupts"
 * specifier.
 */
typedef struct interrupt_t {
	/** @brief Interrupt category (SPI or PPI). */
	irq_type_t type;

	/** @brief Interrupt number within its category. */
	uint32_t id;

	/** @brief Trigger type for this interrupt. */
	irq_trigger_t trigger;
} interrupt_t;

/**
 * @brief Converts a parsed FDT interrupt specifier to its GIC INTID.
 *
 * Per the ARM GIC devicetree binding, the "interrupts" cell's id is an
 * offset within its category rather than the absolute INTID, so the base
 * offset must be added back in: PPIs start at INTID 16, SPIs at INTID 32.
 *
 * @param interrupt Interrupt specifier to convert.
 * @return The GIC INTID, or -1 if @p interrupt has an unrecognized type.
 */
static inline int32_t interrupt_to_intid(const interrupt_t *const interrupt)
{
	if (interrupt->type == IRQ_TYPE_PPI) {
		return (int32_t)(16U + interrupt->id);
	}
	if (interrupt->type == IRQ_TYPE_SPI) {
		return (int32_t)(32U + interrupt->id);
	}

	kprintf("FDT Error: unrecognized interrupt type %d\n", interrupt->type);
	return -1;
}

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
 * @param mmap Pointer to a memory_map_t structure to be populated.
 * @return 0 on success, or a negative libfdt error code on failure.
 */
int get_mem(const void *fdt, memory_map_t *mmap);

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
 * @param reg_map Pointer to a memory_map_t structure to be populated with
 * register information.
 * @return true if the register property is found and successfully parsed, false
 * otherwise.
 */
bool get_reg_property(const void *fdt, int node_offset, memory_map_t *reg_map);

/**
 * @brief Retrieves the interrupt property for a given node.
 * @param fdt Pointer to the FDT header in memory.
 * @param node_offset Offset of the node for which to retrieve the interrupt
 * property.
 * @param out_intr_array Array to be populated with the parsed interrupt
 * specifiers.
 * @param out_intr_array_len Number of elements available in @p out_intr_array.
 * @return The number of interrupt entries parsed (capped at
 * @p out_intr_array_len), or a negative libfdt error code on failure.
 */
int get_intr_property(const void *fdt, int node_offset,
		      interrupt_t *out_intr_array, size_t out_intr_array_len);

/**
 * @brief Finds all nodes in the FDT matching a given "compatible" string.
 * @param fdt Pointer to the FDT header in memory.
 * @param compatible Compatible string to search for.
 * @param out_node_offsets Array to be populated with the offsets of matching
 * nodes.
 * @param out_node_offsets_len Number of elements available in
 * @p out_node_offsets.
 * @return The number of matching nodes found (capped at
 * @p out_node_offsets_len), or a negative libfdt error code on failure.
 */
int get_nodes_by_compatible(const void *fdt, const char *compatible,
			    int *out_node_offsets, size_t out_node_offsets_len);

#endif // FDT_FDT_H
