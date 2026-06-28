/**
 * @file fdt.c
 * @brief Implementation of Device Tree Blob (FDT) parsing and validation
 * functions.
 *
 * Validates the FDT header, extracts memory map entries, and exposes kernel
 * helpers for consuming device tree data.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-16
 */

#include "fdt/fdt.h"

#include "utils/kprintf.h"

#include <stddef.h>
#include <libfdt.h>
#include <stdbool.h>
#include <stdint.h>

bool check_fdt(const void *ptr)
{
	if (!ptr) {
		kprintf("FDT Error: null DTB address\n");
		return false;
	}

	int err = fdt_check_header(ptr);
	switch (err) {
	case 0:
		kprintf("Valid fdt\n");
		return true;
	case -FDT_ERR_BADMAGIC:
		kprintf("FDT Error: Bad magic number\n");
		break;
	case -FDT_ERR_BADVERSION:
		kprintf("FDT Error: Unsupported FDT version\n");
		break;
	case -FDT_ERR_BADSTATE:
		kprintf("FDT Error: Bad state\n");
		break;
	default:
		kprintf("FDT Error: Unknown error code %d\n", err);
		break;
	}
	return false;
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
int get_mem(const void *fdt, Memory_map_t *mmap)
{
	if (!fdt || !mmap) {
		return -FDT_ERR_INTERNAL;
	}

	int node = -1;
	mmap->count = 0;

	/* Find the root node to determine address/size cell requirements */
	int root = fdt_path_offset(fdt, "/");
	if (root < 0) {
		return root;
	}

	int address_cell = fdt_address_cells(fdt, root);
	int size_cell = fdt_size_cells(fdt, root);

	if (address_cell < 1 || address_cell > 2 || size_cell < 1 ||
	    size_cell > 2) {
		kprintf("FDT Error: unsupported address/size cell counts %d/%d\n",
			address_cell, size_cell);
		return -FDT_ERR_BADSTATE;
	}

	/* Search for nodes marked with device_type = "memory".
	 * Standard FDT path is /memory, but some systems use /memory@0 or
	 * others.
	 */
	while ((node = fdt_node_offset_by_prop_value(fdt, node, "device_type",
						     "memory", 7)) >= 0) {
		int len;
		const fdt32_t *reg = fdt_getprop(fdt, node, "reg", &len);

		if (!reg || len <= 0) {
			continue;
		}

		size_t entry_size =
			(size_t)(address_cell + size_cell) * sizeof(fdt32_t);
		if ((size_t)len < entry_size || (len % entry_size) != 0) {
			kprintf("FDT Error: memory \"reg\" length %d is not a multiple of entry size %zu\n",
				len, entry_size);
			return -FDT_ERR_BADSTATE;
		}

		size_t num_entries = (size_t)len / entry_size;

		for (size_t i = 0; i < num_entries; i++) {
			if (mmap->count >= MAX_MEM_REGIONS) {
				break;
			}

			uint64_t base = 0;
			uint64_t size = 0;
			const fdt32_t *ptr =
				reg + (i * (address_cell + size_cell));

			/* Extract address (handles varying cell counts) */
			for (unsigned long j = 0;
			     j < (unsigned long)(address_cell); j++) {
				base = (base << 32) | fdt32_to_cpu(ptr[j]);
			}

			/* Extract size (handles varying cell counts) */
			for (unsigned long j = 0;
			     j < (unsigned long)(size_cell); j++) {
				size = (size << 32) |
				       fdt32_to_cpu(ptr[address_cell + j]);
			}

			/* Ignore zero-sized regions if they appear in the DT */
			if (size > 0) {
				mmap->regions[mmap->count].base = base;
				mmap->regions[mmap->count].size = size;
				mmap->count++;
			}
		}
	}

	return 0;
}
// NOLINTEND(readability-function-cognitive-complexity)

int get_intc_node_offset(const void *fdt)
{
	if (!fdt) {
		return -FDT_ERR_INTERNAL;
	}
	return fdt_node_offset_by_prop_value(fdt, -1, "interrupt-controller",
					     NULL, 0);
}

bool fdt_is_error(int offset)
{
	return offset < 0;
}

char *get_compatible_string(const void *fdt, int node_offset)
{
	if (!fdt || fdt_is_error(node_offset)) {
		return NULL;
	}

	int len;
	const char *compat = fdt_getprop(fdt, node_offset, "compatible", &len);
	if (!compat || len <= 0) {
		return NULL;
	}
	return (char *)compat;
}

bool get_reg_property(const void *fdt, int node_offset, Memory_map_t *reg_map)
{
	if (!fdt || fdt_is_error(node_offset) || reg_map == NULL) {
		return -FDT_ERR_INTERNAL;
	}

	reg_map->count = 0;

	int addr_cells = fdt_address_cells(fdt, node_offset);
	int size_cells = fdt_size_cells(fdt, node_offset);

	int entry_cells = addr_cells + size_cells;
	int len;
	const fdt32_t *reg = fdt_getprop(fdt, node_offset, "reg", &len);
	if (!reg || len <= 0) {
		return false;
	}

	size_t num_entries = len / (entry_cells * sizeof(fdt32_t));
	while (num_entries > 0) {
		uint64_t base = 0;
		uint64_t size = 0;

		for (int i = 0; i < addr_cells; i++) {
			base = (base << 32) | fdt32_to_cpu(*reg++);
		}
		for (int i = 0; i < size_cells; i++) {
			size = (size << 32) | fdt32_to_cpu(*reg++);
		}

		if (reg_map->count < MAX_MEM_REGIONS) {
			reg_map->regions[reg_map->count].base = base;
			reg_map->regions[reg_map->count].size = size;
			reg_map->count++;
		} else {
			kprintf("FDT Warning: reg property has more entries than MAX_MEM_REGIONS, truncating\n");
			return false;
		}
		num_entries--;
	}

	return true;
}

int get_intr_property(const void *fdt, int node_offset,
		      Interrupt_t *out_intr_array, size_t out_intr_array_len)
{
	if (!fdt || fdt_is_error(node_offset) || out_intr_array == NULL) {
		return -FDT_ERR_INTERNAL;
	}

	int len;
	const fdt32_t *intr = fdt_getprop(fdt, node_offset, "interrupts", &len);
	if (!intr || len <= 0) {
		return -FDT_ERR_NOTFOUND;
	}

	/* The cell layout of "interrupts" is defined by whichever node owns
	 * "#interrupt-cells" — found by walking up to the nearest ancestor
	 * with an explicit "interrupt-parent", per the devicetree spec's
	 * inheritance rule. Fall back to the default interrupt controller
	 * if no node in the chain specifies one.
	 */
	int intr_parent_node = -1;
	for (int ancestor = node_offset; ancestor >= 0;
	     ancestor = fdt_parent_offset(fdt, ancestor)) {
		int phandle_len;
		const fdt32_t *phandle_prop = fdt_getprop(
			fdt, ancestor, "interrupt-parent", &phandle_len);
		if (phandle_prop && phandle_len == (int)sizeof(fdt32_t)) {
			intr_parent_node = fdt_node_offset_by_phandle(
				fdt, fdt32_to_cpu(*phandle_prop));
			break;
		}
	}
	if (fdt_is_error(intr_parent_node)) {
		intr_parent_node = get_intc_node_offset(fdt);
	}
	if (fdt_is_error(intr_parent_node)) {
		kprintf("FDT Error: could not resolve an interrupt-parent for node\n");
		return -FDT_ERR_NOTFOUND;
	}

	int cells_len;
	const fdt32_t *cells_prop = fdt_getprop(fdt, intr_parent_node,
						"#interrupt-cells", &cells_len);
	if (!cells_prop || cells_len != (int)sizeof(fdt32_t)) {
		kprintf("FDT Error: interrupt-parent is missing \"#interrupt-cells\"\n");
		return -FDT_ERR_BADNCELLS;
	}
	uint32_t cells_per_entry = fdt32_to_cpu(*cells_prop);

	/* Interrupt_t models the ARM GIC <type, number, flags> layout only;
	 * reject anything else rather than silently misinterpreting it.
	 */
	if (cells_per_entry != 3) {
		kprintf("FDT Error: unsupported #interrupt-cells value %u\n",
			cells_per_entry);
		return -FDT_ERR_BADNCELLS;
	}

	size_t entry_size = (size_t)cells_per_entry * sizeof(fdt32_t);
	if ((size_t)len % entry_size != 0) {
		kprintf("FDT Error: \"interrupts\" length %d is not a multiple of entry size %zu\n",
			len, entry_size);
		return -FDT_ERR_BADSTATE;
	}

	size_t num_entries = (size_t)len / entry_size;
	size_t count = num_entries < out_intr_array_len ? num_entries :
							  out_intr_array_len;

	for (size_t i = 0; i < count; i++) {
		const fdt32_t *entry = intr + (i * cells_per_entry);

		uint32_t type = fdt32_to_cpu(entry[0]);
		uint32_t intr_num = fdt32_to_cpu(entry[1]);
		uint32_t flags = fdt32_to_cpu(entry[2]);

		out_intr_array[i].type = (Irq_type_t)type;
		out_intr_array[i].id = intr_num;
		out_intr_array[i].trigger = (flags & 0x4) ?
						    IRQ_TRIGGER_LEVEL_HIGH :
						    IRQ_TRIGGER_EDGE;
	}

	if (count < num_entries) {
		kprintf("FDT Warning: \"interrupts\" has %lu entries but only %lu fit in out_intr_array, truncating\n",
			(uint64_t)num_entries, (uint64_t)count);
	}

	return (int)count;
}

int get_nodes_by_compatible(const void *fdt, const char *compatible,
			    int *out_node_offsets, size_t out_node_offsets_len)
{
	if (!fdt || !compatible || !out_node_offsets) {
		return -FDT_ERR_INTERNAL;
	}

	int node = -1;
	size_t count = 0;
	while ((node = fdt_node_offset_by_compatible(fdt, node, compatible)) >=
	       0) {
		if (count >= out_node_offsets_len) {
			kprintf("FDT Warning: more \"%s\" compatible nodes than out_node_offsets_len, truncating\n",
				compatible);
			break;
		}
		out_node_offsets[count] = node;
		count++;
	}

	return (int)count;
}
