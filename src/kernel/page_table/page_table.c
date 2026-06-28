/**
 * @file page_table.c
 * @brief Page table support implementation for kernel virtual memory.
 *
 * This module contains the kernel page table implementation used to
 * manage virtual memory mappings and page table creation routines.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-17
 */

#include "page_table/page_table.h"

#include "allocator/page_allocator.h"
#include "utils/kprintf.h"
#include "linker/symbols.h"
#include "linker/linker_defines.h"
#include "fdt/fdt.h"
#include "mem_layout/mem_layout.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Shift for page offset */
#define PAGE_SHIFT 12U

/** Index mask for page table entries */
#define IDX_MASK 0x1FFUL

/** 36-bit field: PA[47:12] */
#define PA_47_12_MASK 0xFFFFFFFFFULL

/** Shift for PA[49:48] */
#define PA_49_48_SHIFT 48U

/** 2-bit field:  PA[49:48] */
#define PA_49_48_MASK 0x3ULL

/** Shift for PA[51:50] */
#define PA_51_50_SHIFT 50U

/** 2-bit field:  PA[51:50] */
#define PA_51_50_MASK 0x3ULL

/**
 * @brief Set the physical Next-Level Table Address (NLTA) in a table
 * descriptor.
 * @param entry Pointer to the table descriptor structure.
 * @param phys_addr The target 4KB-aligned physical address to assign.
 */
static inline void set_page_table_entry_address(table_desc_t *entry,
						const phy_addr phys_addr)
{
	if (!entry) {
		return;
	}

	// 1. Clear any existing address bits to safely allow overwrites
	entry->nlta_47_12 = 0;
	entry->nlta_49_48 = 0;
	entry->nlta_51_50 = 0;

	// 2. Fragment and assign the new physical address bits
	entry->nlta_47_12 = (phys_addr >> PAGE_SHIFT) & PA_47_12_MASK;
	entry->nlta_49_48 = (phys_addr >> PA_49_48_SHIFT) & PA_49_48_MASK;
	entry->nlta_51_50 = (phys_addr >> PA_51_50_SHIFT) & PA_51_50_MASK;
}

/**
 * @brief Set the physical Output Address (OA) in a block or page data
 * descriptor.
 * @param entry Pointer to the page/block data descriptor structure.
 * @param phys_addr The target 4KB-aligned physical address to assign.
 */
static inline void set_page_desc_entry_address(page_desc_t *entry,
					       const phy_addr phys_addr)
{
	if (!entry) {
		return;
	}

	entry->frame_addr_47_12 = 0;
	entry->frame_addr_47_12 = (phys_addr >> PAGE_SHIFT) & PA_47_12_MASK;

	entry->frame_addr_49_48 = 0;
	entry->frame_addr_49_48 = (phys_addr >> PA_49_48_SHIFT) & PA_49_48_MASK;
}

/**
 * @brief Get the physical address from a page table entry.
 * @param entry Pointer to the page table entry.
 * @return The physical address.
 */
static inline phy_addr get_page_table_entry_address(const table_desc_t *entry)
{
	if (!entry->valid) {
		return 0;
	}

	phy_addr phys_addr = 0;

	phys_addr |= ((phy_addr)entry->nlta_47_12) << PAGE_SHIFT;
	phys_addr |= ((phy_addr)entry->nlta_49_48) << PA_49_48_SHIFT;
	phys_addr |= ((phy_addr)entry->nlta_51_50) << PA_51_50_SHIFT;

	return phys_addr;
}

/**
 * @brief Checks if a table descriptor's hierarchical permissions allow the
 * requested page permissions.
 * @param entry Pointer to the target table descriptor structure.
 * @param perms The incoming software page permissions to validate against the
 * table restrictions.
 * @return true if the table permits the mapping rules; false if the table
 * actively restricts them.
 */
static bool check_table_permissions(const table_desc_t *entry,
				    const page_permissions_t perms)
{
	if (!entry || !entry->valid) {
		return false;
	}

	uint8_t ap_table = entry->ap_table;

	// Evaluate user space (EL0) constraints
	if (perms.user_accessible) {
		// Hierarchical codes 0x2 (0b10) and 0x3 (0b11) strip ALL EL0
		// access downstream
		if (ap_table == APTABLE_EL0_NO_ACCESS_EL1_NO_RESTRICTION ||
		    ap_table == APTABLE_EL0_NO_ACCESS_EL1_NO_WRITE) {
			return false;
		}

		// Hierarchical code 0x1 (0b01) strips write capabilities
		// globally down the line
		if (perms.write &&
		    ap_table == APTABLE_EL0_NO_WRITE_EL1_NO_WRITE) {
			return false;
		}
	}
	// Evaluate privileged kernel space (EL1) constraints
	else {
		// Hierarchical codes 0x1 (0b01) and 0x3 (0b11) strip write
		// capabilities from EL1
		if (perms.write) {
			if (ap_table == APTABLE_EL0_NO_WRITE_EL1_NO_WRITE ||
			    ap_table == APTABLE_EL0_NO_ACCESS_EL1_NO_WRITE) {
				return false;
			}
		}
	}

	return true;
}

/**
 * @brief Helper to generate visual indentation based on the current tree depth.
 * @param level The current depth level in the page table tree (0 for root).
 */
static void print_indent(int level)
{
	for (int i = 0; i < level; i++) {
		kprintf("    │   ");
	}
}

/**
 * @brief Initialize a page table.
 * @param table Pointer to the page table to initialize.
 */
static void page_table_init(page_table_t *table)
{
	for (size_t i = 0; i < PTRS_PER_TABLE; i++) {
		table->entries[i].value = 0;
	}
}

/**
 * @brief Set the next level table address in a page table entry union.
 * @param entry Pointer to the root page table entry union to modify.
 * @param next_table_addr Physical address of the next level page table (must be
 * 4KB page aligned).
 */
static void set_next_level_table(page_table_entry_t *entry,
				 phy_addr next_table_addr)
{
	if (!entry) {
		return;
	}

	page_table_entry_t local_entry;
	local_entry.value = 0;

	local_entry.table_desc.valid = 1;
	local_entry.table_desc.is_table = 1;

	set_page_table_entry_address(&local_entry.table_desc, next_table_addr);

	entry->value = local_entry.value;
}

/**
 * @brief Allocate and zero-initialize a new page table, then install it as
 * the next level table in the entry.
 * @param entry Pointer to the page table entry to modify.
 * @return true if successful, false otherwise.
 */
static bool allocate_new_table(table_desc_t *entry)
{
	page_table_t *new_table = (page_table_t *)page_alloc(1);
	if (new_table == NULL) {
		kprintf("Failed to allocate new page table\n");
		return false;
	}

	page_table_init(new_table);

	phy_addr phys_addr = va_to_pa((virt_addr)new_table);

	page_table_entry_t local_entry = { .value = 0 };
	set_next_level_table(&local_entry, phys_addr);

	((page_table_entry_t *)entry)->value = local_entry.value;

	return true;
}

/**
 * @brief Get the next level table from a page table entry, allocating it on
 * demand if the entry is not yet valid.
 * @param entry Pointer to the page table entry.
 * @param perms The requested page permissions for the downstream mapping.
 * @return Pointer to the next level page table, or NULL if @p entry was
 * NULL, allocation of a new table failed, or the existing entry's
 * permissions don't permit the requested page profile.
 */
static page_table_t *get_next_level_table(table_desc_t *entry,
					  const page_permissions_t perms)
{
	if (!entry) {
		return NULL;
	}

	if (!entry->valid) {
		if (!allocate_new_table(entry)) {
			kprintf("Error: Failed to dynamically allocate down-tier page table\n");
			return NULL;
		}
	} else if (!check_table_permissions(entry, perms)) {
		kprintf("Security Violation: Upstream table entry layout restricts requested page profile\n");
		return NULL;
	}

	phy_addr next_table_addr = get_page_table_entry_address(entry);

	// space NOLINTNEXTLINE(*-int-to-ptr)
	return (page_table_t *)pa_to_va(next_table_addr);
}

/**
 * @brief Pretty-prints the contents of a Stage 1 Block/Page descriptor.
 * @param desc The page descriptor structure to decode.
 */
void print_page_descriptor(const page_desc_t desc)
{
	uint64_t physical_address = 0;
	physical_address |= ((uint64_t)desc.frame_addr_47_12) << PAGE_SHIFT;
	physical_address |= ((uint64_t)desc.frame_addr_49_48) << PA_49_48_SHIFT;

	// Decode Shareability (SH) string
	const char *sh_str = "Reserved";
	switch (desc.sh) {
	case SH_NON_SHAREABLE:
		sh_str = "Non-shareable";
		break;
	case SH_OUTER_SHAREABLE:
		sh_str = "Outer Shareable";
		break;
	case SH_INNER_SHAREABLE:
		sh_str = "Inner Shareable";
		break;
	default:
		kprintf("Error: Unknown Shareability\n");
	}

	// Decode Access Permissions (AP[2:1]) string
	const char *ap_str = "Unknown";
	switch (desc.ap) {
	case AP_PRIV_RW:
		ap_str = "EL1: Read/Write, EL0: No Access";
		break;
	case AP_PRIV_UNPRIV_RW:
		ap_str = "EL1: Read/Write, EL0: Read/Write";
		break;
	case AP_PRIV_RO:
		ap_str = "EL1: Read-Only,  EL0: No Access";
		break;
	case AP_PRIV_UNPRIV_RO:
		ap_str = "EL1: Read-Only,  EL0: Read-Only";
		break;
	default:
		kprintf("Error: Unknown AP string\n");
	}

	// Output the decoded structural map
	kprintf("====================================================\n");
	kprintf("  AArch64 Stage 1 %s Descriptor Decode\n",
		desc.is_page ? "PAGE (L3)" : "BLOCK (L1/L2)");
	kprintf("====================================================\n");
	kprintf("  Validity             : %s\n",
		desc.valid ? "VALID" : "INVALID");
	kprintf("  Calculated Output PA : 0x%016llX\n",
		(unsigned long long)physical_address);
	kprintf("  MAIR Attribute Index : %u (AttrIndx[2:0]: %u, AttrIndx[3]: %u)\n",
		(desc.attr_indx_3 << 3) | desc.attr_indx, desc.attr_indx,
		desc.attr_indx_3);
	kprintf("  Security State       : %s\n",
		desc.non_secure ? "Non-Secure (NS)" : "Secure / Realm");
	kprintf("  Access Permissions   : [0x%X] %s\n", desc.ap, ap_str);
	kprintf("  Shareability         : [0x%X] %s\n", desc.sh, sh_str);
	kprintf("  Access Flag (AF)     : %u (%s)\n", desc.access_flag,
		desc.access_flag ? "Accessed" : "Fault on Access");
	kprintf("  Global Mapping (nG)  : %s\n",
		desc.non_global ? "Local (ASID-specific)" : "Global");

	kprintf("------------------ Lower & Upper Attributes ---------\n");
	kprintf("  Guarded Page (BTI)   : %s\n",
		desc.guarded_page ? "Enabled" : "Disabled");
	kprintf("  Dirty Bit Mod (DBM)  : %s\n",
		desc.dirty_bit ? "Dirty / Enabled" : "Clean / Disabled");
	kprintf("  Contiguous Hint      : %s\n",
		desc.contiguous ? "Set (Coalesce TLB)" : "Not Set");
	kprintf("  Execute-Never (PXN)  : %s\n",
		desc.pxn ? "YES (Privileged XN)" : "NO");
	kprintf("  Execute-Never (UXN/XN): %s\n",
		desc.xn ? "YES (Execute-Never)" : "NO");

	kprintf("------------------ System Extensions ----------------\n");
	kprintf("  Software Reserved    : 0x%X\n", desc.sw_reserved);
	kprintf("  POIndex / PBHA[3:1]  : 0x%X\n", desc.po_index);
	kprintf("  AMEC / MECID Bit     : %u\n", desc.amec);
	kprintf("====================================================\n");
}

/**
 * @brief Pretty-prints the contents of a Stage 1 Table descriptor (Levels 0, 1,
 * or 2).
 * @param desc The table descriptor structure to decode.
 */
void print_table_descriptor(const table_desc_t desc)
{
	uint64_t next_table_address = 0;
	next_table_address |= ((uint64_t)desc.nlta_47_12) << PAGE_SHIFT;
	next_table_address |= ((uint64_t)desc.nlta_49_48) << PA_49_48_SHIFT;
	next_table_address |= ((uint64_t)desc.nlta_51_50) << PA_51_50_SHIFT;

	// Decode Hierarchical Access Permissions (APTable[1:0])
	const char *ap_table_str = "No effect on downstream permissions";
	switch (desc.ap_table) {
	case APTABLE_EL0_NO_RESTRICTION_EL1_NO_RESTRICTION:
		ap_table_str = "No effect on downstream permissions";
		break;
	case APTABLE_EL0_NO_ACCESS_EL1_NO_RESTRICTION:
		ap_table_str =
			"Downstream EL0 access restricted (Forces AP[1] to 0 block/page side)";
		break;
	case APTABLE_EL0_NO_WRITE_EL1_NO_WRITE:
		ap_table_str =
			"Downstream Write access restricted (Forces Read-Only down the chain)";
		break;
	case APTABLE_EL0_NO_ACCESS_EL1_NO_WRITE:
		ap_table_str = "Downstream Read-Only + EL0 restricted combined";
		break;
	default:
		kprintf("Error: Unknown AP string\n");
	}

	// Output the decoded structural map
	kprintf("====================================================\n");
	kprintf("  AArch64 Stage 1 TABLE (L0/L1/L2) Descriptor Decode\n");
	kprintf("====================================================\n");
	kprintf("  Validity             : %s\n",
		desc.valid ? "VALID" : "INVALID");
	kprintf("  Type Bit (is_table)  : %u (%s)\n", desc.is_table,
		desc.is_table ? "Table Link" : "FAULT (Should be Block)");
	kprintf("  Next-Level Table PA  : 0x%016llX\n",
		(unsigned long long)next_table_address);
	kprintf("  Table Access Flag(AF): %u (%s)\n", desc.access_flag,
		desc.access_flag ? "Enabled/Set" : "Not Set / Ignored");

	kprintf("------------------ Hierarchical Controls -----------\n");
	kprintf("  NSTable (Security)   : %s\n",
		desc.ns_table ? "Force Non-Secure downstream" :
				"Follow root security domain");
	kprintf("  APTable (Permissions): [0x%X] %s\n", desc.ap_table,
		ap_table_str);
	kprintf("  XNTable (Exec-Never) : %s\n",
		desc.xn_table ? "Force Execute-Never downstream (UXN/XN)" :
				"No structural execution limit");
	kprintf("  PXNTable (Priv-XN)   : %s\n",
		desc.pxn_table ? "Force Privileged Execute-Never downstream" :
				 "No privileged execution limit");

	kprintf("------------------ Hardware / Feature Bit States ----\n");
	kprintf("  Protected (PnCH)     : %s\n",
		desc.protected ? "Stage 1 Protected Attribute Active" :
				 "Disabled / Ignored");
	kprintf("  RES0 Check (Bit 50)  : %u\n", desc.res_50);

	kprintf("------------------ Reserved / Ignored Blocks --------\n");
	kprintf("  Ignored Bits [7:2]   : 0x%02X\n", desc.ignored_7_2);
	kprintf("  Ignored Bit [11]     : %u\n", desc.ignored_11);
	kprintf("  Ignored Bit [51]     : %u\n", desc.ignored_51);
	kprintf("  Ignored Bits [58:53] : 0x%02X\n", desc.ignored_58_53);
	kprintf("====================================================\n");
}

/**
 * @brief Recursively prints the structure of the page table tree starting from
 * a given base table.
 * @param table_base Pointer to the base of the current page table level.
 * @param current_level The current level in the page table hierarchy (0-3).
 * @param base_va The starting virtual address range covered by this table.
 */
// Let the cognitive complexity be there as it will be easier during debugging.
// NOLINTBEGIN(*-recursion,*-cognitive-complexity,*-nested-conditional-operator)
void print_page_table_tree(const page_table_entry_t *table_base,
			   int current_level, uint64_t base_va)
{
	if (!table_base || current_level > 3) {
		return;
	}

	const int num_entries = 512;
	uint64_t va_stride = 1ULL << (12 + (3 - current_level) * 9);

	print_indent(current_level);
	kprintf("|--> Entering LEVEL %d Table [Base VA Range: 0x%lx]\n",
		current_level, base_va);

	for (int i = 0; i < num_entries; i++) {
		page_table_entry_t entry = table_base[i];

		if (!entry.generic.valid) {
			continue;
		}

		uint64_t entry_va_start = base_va + (i * va_stride);
		uint64_t entry_va_end = entry_va_start + va_stride - 1;

		print_indent(current_level + 1);
		kprintf("|-- Entry [%d] (VA: 0x%lx -> 0x%lx) ", i,
			entry_va_start, entry_va_end);

		// ==========================================
		// LEVEL 3: LEAF PAGE DESCRIPTORS
		// ==========================================
		if (current_level == 3) {
			uint64_t phy_addr =
				((uint64_t)entry.page_desc.frame_addr_47_12
				 << PAGE_SHIFT) |
				((uint64_t)entry.page_desc.frame_addr_49_48
				 << PA_49_48_SHIFT);

			// Decode Page Attributes
			const char *ap_str =
				(entry.page_desc.ap == AP_PRIV_RW) ? "EL1_RW" :
				(entry.page_desc.ap == AP_PRIV_UNPRIV_RW) ?
								     "EL01_RW" :
				(entry.page_desc.ap == AP_PRIV_RO) ? "EL1_RO" :
								     "EL01_RO";

			const char *xn_str =
				(entry.page_desc.pxn && entry.page_desc.xn) ?
					"NX" :
				(entry.page_desc.pxn) ? "PXN" :
				(entry.page_desc.xn)  ? "UXN" :
							"X";

			const char *sh_str =
				(entry.page_desc.sh == SH_INNER_SHAREABLE) ?
					"Inner-Shareable" :
				(entry.page_desc.sh == SH_OUTER_SHAREABLE) ?
					"Outer-Shareable" :
					"Non-Shareable";

			kprintf("[PAGE] --> Maps to Output PA: 0x%lx [%s, %s, %s, AF:%d]\n",
				phy_addr, ap_str, xn_str, sh_str,
				entry.page_desc.access_flag);
		}
		// ==========================================
		// LEVELS 0, 1, 2: TABLES OR BLOCKS
		// ==========================================
		else {
			if (entry.generic.type == 1) {
				// Table Descriptor
				uint64_t next_table_pa =
					((uint64_t)entry.table_desc.nlta_47_12
					 << PAGE_SHIFT) |
					((uint64_t)entry.table_desc.nlta_49_48
					 << PA_49_48_SHIFT) |
					((uint64_t)entry.table_desc.nlta_51_50
					 << PA_51_50_SHIFT);

				const char *apt_str =
					(entry.table_desc.ap_table ==
					 APTABLE_EL0_NO_RESTRICTION_EL1_NO_RESTRICTION) ?
						"No-Restrict" :
					(entry.table_desc.ap_table ==
					 APTABLE_EL0_NO_ACCESS_EL1_NO_RESTRICTION) ?
						"Table-EL0-NA" :
					(entry.table_desc.ap_table ==
					 APTABLE_EL0_NO_WRITE_EL1_NO_WRITE) ?
						"Table-RO" :
						"Table-EL0-NA_EL1-RO";

				kprintf("[TABLE LINK] --> Next Table PA: 0x%lx [Hierarchical: %s]\n",
					next_table_pa, apt_str);

				page_permissions_t dump_perms = {
					.read = true,
					.write = true,
					.execute = true,
					.user_accessible = true
				};

				const page_table_entry_t *next_table_sys_ptr =
					(page_table_entry_t *)
						get_next_level_table(
							&entry.table_desc,
							dump_perms);

				// Recurse down
				print_page_table_tree(next_table_sys_ptr,
						      current_level + 1,
						      entry_va_start);
			} else {
				// Block Descriptor (1GB Map at L1 or 2MB Map at
				// L2)
				uint64_t phy_addr = ((uint64_t)entry.page_desc
							     .frame_addr_47_12
						     << PAGE_SHIFT) |
						    ((uint64_t)entry.page_desc
							     .frame_addr_49_48
						     << PA_49_48_SHIFT);

				const char *ap_str =
					(entry.page_desc.ap == AP_PRIV_RW) ?
						"EL1_RW" :
					(entry.page_desc.ap ==
					 AP_PRIV_UNPRIV_RW) ?
						"EL01_RW" :
					(entry.page_desc.ap == AP_PRIV_RO) ?
						"EL1_RO" :
						"EL01_RO";

				const char *xn_str =
					(entry.page_desc.pxn &&
					 entry.page_desc.xn) ?
						"NX" :
					(entry.page_desc.pxn) ? "PXN" :
					(entry.page_desc.xn)  ? "UXN" :
								"X";

				const char *sh_str = (entry.page_desc.sh ==
						      SH_INNER_SHAREABLE) ?
							     "Inner-Shareable" :
						     (entry.page_desc.sh ==
						      SH_OUTER_SHAREABLE) ?
							     "Outer-Shareable" :
							     "Non-Shareable";

				kprintf("[BLOCK MAP] --> Maps huge block to Physical PA: 0x%lx [%s, %s, %s, AF:%d]\n",
					phy_addr, ap_str, xn_str, sh_str,
					entry.page_desc.access_flag);
			}
		}
	}
}
// NOLINTEND(*-recursion,*-cognitive-complexity,*-nested-conditional-operator)

bool map_page(page_table_t *root, const virt_addr v_addr,
	      const phy_addr phy_addr, const page_permissions_t perms,
	      const enum mem_type_t mem_type)
{
	if ((phy_addr & 0xFFF) || (v_addr & 0xFFF)) {
		kprintf("Error: Addresses must be 4KB page aligned (VA: 0x%lx, PA: 0x%lx)\n",
			v_addr, phy_addr);
		return false;
	}

	if (!root) {
		return false;
	}

	const uint64_t idx_mask = 0x1FF; // 9 bits
	const uint64_t l0_idx = (v_addr >> 39) & idx_mask;
	const uint64_t l1_idx = (v_addr >> 30) & idx_mask;
	const uint64_t l2_idx = (v_addr >> 21) & idx_mask;
	const uint64_t l3_idx = (v_addr >> 12) & idx_mask;

	page_table_t *level1 =
		get_next_level_table(&root->entries[l0_idx].table_desc, perms);
	if (!level1) {
		kprintf("Error: Failed to fetch or allocate L1 table for VA 0x%lx\n",
			v_addr);
		return false;
	}

	page_table_t *level2 = get_next_level_table(
		&level1->entries[l1_idx].table_desc, perms);
	if (!level2) {
		kprintf("Error: Failed to fetch or allocate L2 table for VA 0x%lx\n",
			v_addr);
		return false;
	}

	page_table_t *level3 = get_next_level_table(
		&level2->entries[l2_idx].table_desc, perms);
	if (!level3) {
		kprintf("Error: Failed to fetch or allocate L3 table for VA 0x%lx\n",
			v_addr);
		return false;
	}

	level3->entries[l3_idx].value = 0;

	page_desc_t *page_desc = &level3->entries[l3_idx].page_desc;

	page_desc->valid = 1;
	page_desc->is_page = 1; // Explicitly confirms L3 4KB target allocation

	set_page_desc_entry_address(page_desc, phy_addr);

	if (perms.execute) {
		page_desc->xn = 0; // Clear Execute-Never
		page_desc->pxn = 0; // Clear Privileged Execute-Never
	} else {
		page_desc->xn = 1;
		page_desc->pxn = 1;
	}

	if (!perms.read) {
		page_desc->valid = 0; // If there is no read clearance,
				      // mathematically unmap the slot entirely
	} else if (perms.write) {
		page_desc->ap = perms.user_accessible ? AP_PRIV_UNPRIV_RW :
							AP_PRIV_RW;
	} else {
		page_desc->ap = perms.user_accessible ? AP_PRIV_UNPRIV_RO :
							AP_PRIV_RO;
	}

	page_desc->access_flag = 1;

	if (mem_type == device) {
		page_desc->attr_indx = device;
		page_desc->sh = SH_NON_SHAREABLE;
	} else {
		page_desc->attr_indx = normal; // Normal WB (MAIR slot 1)
		page_desc->sh = SH_INNER_SHAREABLE;
	}

	return true;
}

void dump_memory_map(page_table_t *root)
{
	kprintf("--- Memory Map Dump Start ---\n");
	print_page_table_tree(root->entries, 0, 0);
	kprintf("--- Memory Map Dump End ---\n");
}

bool setup_kernel_id_map(void)
{
	void *idmap_pg_dir_start_addr = get_id_map_root();

	// Initialize the page tracking block allocated for the ID map
	// structures
	bool page_init_result =
		page_init(va_to_pa((virt_addr)idmap_pg_dir_start_addr),
			  (size_t)ID_MAP_SIZE);
	if (!page_init_result) {
		kprintf("Error: Failed while setting up initial static page idmap pool\n");
		return false;
	}

	// Allocate the root Level 0 page table frame from the newly
	// initialized pool. This should be intentionally done first after the
	// page allocator is initialized
	page_table_t *root = (page_table_t *)page_alloc(1);
	if (root == NULL ||
	    // let the check be here to ensure we are using the right map
	    ((void *)root != idmap_pg_dir_start_addr)) {
		kprintf("Error: Failed while allocating initial static page idmap root\n");
		return false;
	}

	const page_permissions_t kernel_exe_perms = { .read = true,
						      .write = true,
						      .execute = true,
						      .user_accessible =
							      false };

	uintptr_t cur_vaddr = (uintptr_t)&image_start;
	uintptr_t end_vaddr = (uintptr_t)&image_end;

	kprintf("MMU: Creating Identity Map (0x%lx -> 0x%lx)\n", cur_vaddr,
		end_vaddr);

	while (cur_vaddr < end_vaddr) {
		if (!map_page(root, cur_vaddr, cur_vaddr, kernel_exe_perms,
			      normal)) {
			kprintf("Error: Failed while mapping page at vaddr/paddr: 0x%lx\n",
				cur_vaddr);
			return false;
		}
		cur_vaddr += PAGE_SIZE;
	}

	return true;
}

bool setup_kernel_map(memory_map_t *const mmap)
{
	// Todo: In the future, we can extend this function to support multiple
	// memory.
	if (mmap == NULL || mmap->count != 1) {
		kprintf("Error: Invalid memory map provided for kernel mapping\n");
		return false;
	}

	page_table_t *kernel_map_root = get_kernel_map_root();

	page_table_init(kernel_map_root);

	uintptr_t cur_phy_addr = mmap->regions[0].base;
	uintptr_t end_phy_addr =
		(uintptr_t)mmap->regions[0].base + mmap->regions[0].size;

	kprintf("MMU: Creating kernel Map (0x%lx -> 0x%lx)\n", cur_phy_addr,
		end_phy_addr);

	const page_permissions_t rw_perms = { .read = true,
					      .write = true,
					      .execute = false,
					      .user_accessible = false };
	const page_permissions_t rwx_perms = { .read = true,
					       .write = true,
					       .execute = true,
					       .user_accessible = false };

	while (cur_phy_addr < end_phy_addr) {
		page_permissions_t perms = rw_perms;
		if (cur_phy_addr >= (uintptr_t)&image_start &&
		    cur_phy_addr < (uintptr_t)&image_end) {
			perms = rwx_perms;
			// For the kernel code region, give RWX
			// (RW is needed to allow for self-modifying code
			// patterns like patching the page tables themselves,
			// and X is needed for execution) Note: The kernel data
			// region is also given RWX permissions here for
			// simplicity, but in a more security-conscious design,
			// you might want to separate the code and data regions
			// and give them different permissions (e.g., RX for
			// code and RW for data). For this example, we
		}
		if (!map_page(kernel_map_root, cur_phy_addr + KERNEL_BASE,
			      cur_phy_addr, perms, normal)) {
			kprintf("Error: Failed while mapping page at vaddr/paddr: 0x%lx\n",
				cur_phy_addr);
			return false;
		}

		cur_phy_addr += PAGE_SIZE;
	}

	return true;
}
