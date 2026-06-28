/**
 * @file page_table.h
 * @brief Page table definitions and helpers for virtual memory mapping.
 *
 * Provides architecture-specific macros and declarations for building and
 * manipulating page tables used by the kernel's memory management.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-17
 */

#ifndef PAGE_TABLE_PAGE_TABLE_H
#define PAGE_TABLE_PAGE_TABLE_H

#include "linker/linker_defines.h"
#include "fdt/fdt.h"
#include "mem_layout/mem_layout.h"

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Size of each memory page in bytes.
 */
#define PAGE_SIZE LINKER_PAGE_SIZE

/**
 * @brief Number of pointers per page table.
 * from  table D4-21 in the Armv8-A architecture reference manual.
 */
#define PTRS_PER_TABLE 512

/**
 * @brief Software representation of architectural page mapping privileges.
 *
 * This tracking structure is used by the Virtual Memory Manager (VMM) to
 * translate abstract software permissions down into raw ARM64 hardware bitfield
 * constraints.
 */
typedef struct page_permissions_t {
	/** [Bit 0] Read clearance bit. 1 = Read Allowed, 0 = No Read Access. */
	bool read : 1;

	/** [Bit 1] Write clearance bit. 1 = Writable, 0 = Read-Only. */
	bool write : 1;

	/** [Bit 2] Execution clearance bit. 1 = Executable, 0 =
	 * Execute-Never (XN). */
	bool execute : 1;

	/** [Bit 3] Privilege Level bit.   1 = EL0 (User), 0 = EL1 Only
	 * (Kernel). */
	bool user_accessible : 1;
} page_permissions_t;

/** @brief Memory type used as an index into the MAIR_EL1 register.
 *
 *  The numeric value of each enumerator directly corresponds to the MAIR_EL1
 *  attribute index encoded in page/block descriptor bits [4:2] (AttrIndx).
 *  The kernel must program MAIR_EL1 so that each index carries the correct
 *  attribute byte before the MMU is enabled.
 */
enum mem_type_t {
	/** Index 0 — MAIR_EL1[7:0]:  0x00  Device-nGnRnE (non-gathering,
	   non-reordering, no early write-ack). */
	device = 0,
	/** Index 1 — MAIR_EL1[15:8]: 0xFF  Normal Inner/Outer Write-Back
	   Read/Write-Allocate Cacheable. */
	normal,
};

/** @brief TABLE DESCRIPTOR LAYOUT (Lookup levels 0, 1, or 2).
 *
 *  Represents a 64-bit table descriptor as defined by ARMv8/ARMv9
 *  VMSAv8-64 Table D8-50 (4 KB granule).  A descriptor at levels 0–2
 *  whose bits [1:0] == 0b11 points to the next-level page table whose
 *  base address is encoded in nlta_47_12 (and optionally the DS
 *  extension fields).  Hierarchical permission fields in bits [63:59]
 *  propagate constraints down to every leaf reached through this table.
 */
typedef struct __attribute__((packed)) table_desc_t {
	/** [Bit 0]  Valid descriptor — must be 1; a 0 causes a translation
	 * fault. */
	uint64_t valid : 1;

	/** [Bit 1]  Descriptor type — must be 1 to identify this as a table
	 * descriptor (levels 0–2).  Combined with the `valid` bit gives bits
	 * [1:0]
	 * == 0b11. */
	uint64_t is_table : 1;

	/** [Bits 7:2]  IGNORED by hardware; available for software use. */
	uint64_t ignored_7_2 : 6;

	/** [Bits 9:8]  NLTA[51:50] — upper bits of the next-level table address
	 * when TCR_ELx.DS == 1 (52-bit PA support); otherwise IGNORED. */
	uint64_t nlta_51_50 : 2;

	/** [Bit 10]  Access Flag (AF) — set by hardware on first access when
	 *            FEAT_HAFDBS hardware management is enabled; otherwise
	 * IGNORED. */
	uint64_t access_flag : 1;

	/** [Bit 11]  IGNORED by hardware; available for software use. */
	uint64_t ignored_11 : 1;

	/** [Bits 47:12]  Next-Level Table Address — OA[47:12] of the 4
	 * KB-aligned physical address of the subordinate translation table. */
	uint64_t nlta_47_12 : 36;

	/** [Bits 49:48]  NLTA[49:48] — additional PA bits when TCR_ELx.DS == 1;
	 *                otherwise RES0. */
	uint64_t nlta_49_48 : 2;

	/** [Bit 50]  RES0 — reserved, must be written as 0. */
	uint64_t res_50 : 1;

	/** [Bit 51]  IGNORED by hardware; available for software use. */
	uint64_t ignored_51 : 1;

	/** [Bit 52]  Protected Attribute — valid when FEAT_RME PnCH == 1;
	 * otherwise IGNORED. */
	uint64_t protected : 1;

	/** [Bits 58:53]  IGNORED by hardware; available for software use. */
	uint64_t ignored_58_53 : 6;

	/** [Bit 59]  PXNTable — hierarchical Privileged-Execute-Never limit
	 * applied to all leaf descriptors reached through this table.  RES0
	 * when the stage does not support PXN (e.g. EL2 non-secure stage 1). */
	uint64_t pxn_table : 1;

	/** [Bit 60]  XNTable / UXNTable — hierarchical Execute-Never limit for
	 *            unprivileged (EL0) execution applied to all leaves below
	 * this table. */
	uint64_t xn_table : 1;

	/** [Bits 62:61]  APTable[1:0] — hierarchical access-permission limit.
	 *                Overrides AP[2:1] in leaf descriptors reached through
	 * this table: 00 = no effect, 01 = no EL0 access, 10 = read-only all,
	 *                11 = read-only EL1 only. */
	uint64_t ap_table : 2;

	/** [Bit 63]  NSTable — output address is in Non-secure PA space when 1
	 * (Secure state only); RES0 in Non-secure state. */
	uint64_t ns_table : 1;
} table_desc_t;

/** @brief FLAT BLOCK/PAGE DATA DESCRIPTOR LAYOUT — maps real physical memory.
 *
 *  Represents a 64-bit Stage 1 VMSAv8-64 block or page descriptor (4 KB
 * granule) as defined in ARMv8/ARMv9 Architecture Reference Manual.
 *
 *  - **Level 1/2 block** — bits[1:0] == 0b01, maps a 1 GB (L1) or 2 MB (L2)
 * region.
 *  - **Level 3 page**    — bits[1:0] == 0b11, maps a 4 KB page.
 *
 *  Lower attributes (bits [11:2]) control memory type, permissions, and
 * shareability. The output address window (bits [49:12]) holds the physical
 * frame address. Upper attributes (bits [63:50]) carry execution controls and
 * feature extensions.
 */
typedef struct __attribute__((packed)) page_desc_t {
	/** [Bit 0]  Valid — must be 1; a 0 causes a translation fault. */
	uint64_t valid : 1;

	/** [Bit 1]  Descriptor type — 0 = block (L1/L2), 1 = page (L3).
	 *           Combined with valid: L1/L2 block = 0b01, L3 page =
	 * 0b11. */
	uint64_t is_page : 1;

	/** [Bits 4:2]  AttrIndx[2:0] — index into MAIR_EL1 selecting the memory
	 *              attribute (see @ref mem_type_t for the kernel's slot
	 * assignments). */
	uint64_t attr_indx : 3;

	/** [Bit 5]  NS — Non-Secure PA space selection (Secure state only);
	 * RES0 in Non-secure state. */
	uint64_t non_secure : 1;

	/** [Bits 7:6]  AP[2:1] — Data Access Permissions.
	 *              00 = EL1 RW / EL0 none, 01 = EL1+EL0 RW,
	 *              10 = EL1 RO / EL0 none, 11 = EL1+EL0 RO. */
	uint64_t ap : 2;

	/** [Bits 9:8]  SH[1:0] — Shareability domain for Normal memory.
	 *              00 = Non-shareable, 10 = Outer Shareable, 11 = Inner
	 * Shareable. (01 is reserved.) */
	uint64_t sh : 2;

	/** [Bit 10]  AF — Access Flag.  Must be 1 when software manages the
	 * flag; set automatically by hardware when FEAT_HAFDBS is enabled. */
	uint64_t access_flag : 1;

	/** [Bit 11]  nG — Non-Global.  When 1, TLB entries are tagged with the
	 * current ASID and are not global; used for process-private mappings.
	 */
	uint64_t non_global : 1;

	/** [Bits 47:12]  OA[47:12] — Output (physical) address of the 4
	 * KB-aligned frame or block base (bits 11:0 are always 0 for 4 KB
	 * granule). */
	uint64_t frame_addr_47_12 : 36;

	/** [Bits 49:48]  OA[49:48] — Additional PA bits when TCR_ELx.DS == 1
	 *                (FEAT_LPA, 52-bit PA); otherwise RES0. */
	uint64_t frame_addr_49_48 : 2;

	/** [Bit 50]  GP — Guarded Page (FEAT_BTI).  When 1, the CPU enforces
	 * Branch Target Identification on indirect branches into this page. */
	uint64_t guarded_page : 1;

	/** [Bit 51]  DBM — Dirty Bit Modifier (FEAT_HAFDBS).  When 1, hardware
	 *            clears AP[2] on first write, marking the page dirty;
	 * otherwise this bit is available as a software dirty flag. */
	uint64_t dirty_bit : 1;

	/** [Bit 52]  Contiguous — hint to the TLB that this entry is part of a
	 *            naturally-aligned contiguous set (16 × 4 KB for 4 KB
	 * granule), allowing a single TLB entry to cover the whole range. */
	uint64_t contiguous : 1;

	/** [Bit 53]  PXN — Privileged Execute-Never.  When 1, execution at EL1
	 * from this region causes a Permission fault. */
	uint64_t pxn : 1;

	/** [Bit 54]  UXN / XN — Unprivileged Execute-Never.  When 1, execution
	 * at EL0 (and at all levels for non-stage-1 descriptors) is forbidden.
	 */
	uint64_t xn : 1;

	/** [Bits 58:55]  IGNORED / software use — hardware does not interpret
	 * these bits; available for OS-defined flags (e.g. swap, COW markers).
	 */
	uint64_t sw_reserved : 4;

	/** [Bit 59]  AttrIndx[3] (FEAT_AIE) or PBHA[0] (FEAT_HPDS2) — extends
	 * the MAIR index to 4 bits when FEAT_AIE is implemented; otherwise
	 *            Page-Based Hardware Attribute bit 0. */
	uint64_t attr_indx_3 : 1;

	/** [Bits 62:60]  POIndex[2:0] (FEAT_S1POE) or PBHA[3:1] (FEAT_HPDS2) —
	 *                Protection Object index for overlay permissions when
	 *                FEAT_S1POE is active; otherwise Page-Based Hardware
	 * Attribute bits [3:1] passed to the interconnect. */
	uint64_t po_index : 3;

	/** [Bit 63]  AMEC / nse — Realm Management Extension MECID control bit
	 *            (FEAT_RME); selects the Memory Encryption Context for this
	 * page. RES0 when FEAT_RME is not implemented. */
	uint64_t amec : 1;
} page_desc_t;

/**
 * @brief Flat, architecturally stable AArch64 Stage 1 Descriptor structure.
 *
 * This union maps a standard 64-bit ARMv8/ARMv9 translation table descriptor.
 * By avoiding nested internal anonymous unions and leveraging flattened
 * structural representations, it guarantees exact 8-byte compilation across GCC
 * and Clang toolchains without unintended alignment padding.
 */
typedef union __attribute__((packed)) page_table_entry_t {
	/** * @var value
	 * @brief Raw 64-bit integer presentation of the full descriptor.
	 * Useful for atomic page table writes or direct register assignments.
	 */
	uint64_t value;

	/**
	 * @brief Raw hardware bits used for quick validity and node type
	 * assessment.
	 * * Allows low-overhead pre-checking during recursive page table walks
	 * before casting to specific descriptor variations.
	 */
	struct __attribute__((packed)) {
		/** [0] Valid descriptor bit. 1 = Active, 0 = Fault on
		 * access. */
		uint64_t valid : 1;

		/** [1] Descriptor Type block bit. 1 = Table/Page, 0 =
		 * Block/Reserved. */
		uint64_t type : 1;

		/** [63:2] Untouched bits representing remaining
		 * descriptor space. */
		uint64_t rest : 62;
	} generic; /// Used for quick checks without needing to interpret the
		   /// full descriptor.

	/**
	 * @var table_desc
	 * @brief Structured layout for translation table descriptors (Levels 0,
	 * 1, or 2). Points to the base address of the next-level lookup table
	 * frame.
	 */
	table_desc_t table_desc;

	/**
	 * @var page_desc
	 * @brief Structured layout for block mappings (L1/L2) or page mappings
	 * (L3). Maps physical memory output windows directly.
	 */
	page_desc_t page_desc;

} page_table_entry_t;

/**
 * @brief Access Permission Table bits (APTable[1:0]) for Stage 1 Table
 * Descriptors.
 *
 * This hierarchical control restricts the maximum permissions allowed for all
 * downstream block or page descriptors reachable through this table entry.
 *
 * @note These controls can only *restrict* permissions down the chain; they
 * cannot grant more privileges than what a leaf L3 page descriptor specifies.
 *
 * +------+----------------------------+----------------------------+
 * |Value | EL0 (Unprivileged)         | EL1 (Privileged)           |
 * +------+----------------------------+----------------------------+
 * | 0b00 | No restriction             | No restriction             |
 * | 0b01 | No Access                  | No restriction             |
 * | 0b10 | Read-Only (No write)       | Read-Only (No write)       |
 * | 0b11 | No Access                  | Read-Only (No write)       |
 * +------+----------------------------+----------------------------+
 *
 * @note APTable[0] is treated as 0 when virtualization is active and
 * HCR_EL2.{NV,NV1} == {1,1}.
 */
enum aptable_values {
	/** [0b00] No downstream permission modifications applied. */
	APTABLE_EL0_NO_RESTRICTION_EL1_NO_RESTRICTION = 0b00,

	/** [0b01] APTable[0]=1: strip EL0 access entirely. EL1
	   unaffected. */
	APTABLE_EL0_NO_ACCESS_EL1_NO_RESTRICTION = 0b01,

	/** [0b10] APTable[1]=1: force read-only for all ELs. */
	APTABLE_EL0_NO_WRITE_EL1_NO_WRITE = 0b10,

	/** [0b11] Highly restricted. EL0: No Access, EL1: Forced
	   Read-Only. */
	APTABLE_EL0_NO_ACCESS_EL1_NO_WRITE = 0b11
};

/**
 * @brief AP[2:1] data access permission encodings for Stage 1 leaf descriptors.
 *
 * Encodes the direct permissions written into bits [7:6] of a block or page
 * descriptor.  For a stage 1 translation supporting two Exception levels
 * (Table D8-63, ARMv8/ARMv9 ARM):
 *
 * +----------+-------------------------------------------+
 * | AP[2:1]  | Permissions                               |
 * +----------+-------------------------------------------+
 * |   0b00   | PrivRead, PrivWrite                       |
 * |   0b01   | PrivRead, PrivWrite, UnprivRead, UnprivWrite |
 * |   0b10   | PrivRead                                  |
 * |   0b11   | PrivRead, UnprivRead                      |
 * +----------+-------------------------------------------+
 *
 * "Priv" == EL1 (kernel), "Unpriv" == EL0 (user).
 */
enum ap_values {
	/** [0b00] EL1 read/write. EL0 no access. */
	AP_PRIV_RW = 0b00,
	/** [0b01] EL1 and EL0 read/write. */
	AP_PRIV_UNPRIV_RW = 0b01,
	/** [0b10] EL1 read-only. EL0 no access. */
	AP_PRIV_RO = 0b10,
	/** [0b11] EL1 and EL0 read-only. */
	AP_PRIV_UNPRIV_RO = 0b11,
};

/**
 * @brief SH[1:0] shareability domain encodings for Stage 1 leaf descriptors.
 *
 * Written into bits [9:8] of a block or page descriptor.  Controls which
 * observers share the coherency domain for Normal memory accesses.
 * For Device memory the shareability is always Outer Shareable regardless
 * of this field.
 *
 * +--------+------------------+
 * | SH[1:0]| Domain           |
 * +--------+------------------+
 * |  0b00  | Non-shareable    |
 * |  0b01  | RESERVED         |
 * |  0b10  | Outer Shareable  |
 * |  0b11  | Inner Shareable  |
 * +--------+------------------+
 */
enum sh_values {
	/** [0b00] Non-shareable — no coherency requirement with other
	   observers. */
	SH_NON_SHAREABLE = 0b00,
	/** [0b01] RESERVED — must not be used. */
	SH_RESERVED = 0b01,
	/** [0b10] Outer Shareable — coherent with the outer shareability domain
	 *         (typically all CPUs + GPU + DMA-capable devices). */
	SH_OUTER_SHAREABLE = 0b10,
	/** [0b11] Inner Shareable — coherent within the inner shareability
	 * domain (typically all CPUs in the same cluster). */
	SH_INNER_SHAREABLE = 0b11,
};

/**
 * @struct page_table_t
 * @brief Represents a single page table level (512 entries).
 */
typedef struct page_table_t {
	/** Array of page table entries. */
	page_table_entry_t entries[PTRS_PER_TABLE];
} page_table_t;

/**
 * @brief Map a virtual address to a physical address in the page table.
 *
 * Walks the four-level (L0→L3) translation table rooted at @p root,
 * allocating intermediate table descriptors as needed, and installs a
 * leaf page descriptor at L3 that covers the 4 KB page containing
 * @p v_addr.  The descriptor's AttrIndx field is set from @p mem_typ,
 * selecting the matching MAIR_EL1 slot (see @ref mem_type_t).
 *
 * @param root     Pointer to the root (L0) page table.  Must not be NULL.
 * @param v_addr   Virtual address to map; only bits [47:12] are significant
 *                 (the 4 KB page base — lower bits are ignored).
 * @param phy_addr Physical address of the target frame; must be 4 KB-aligned.
 * @param perms    Software permission flags (@ref page_permissions_t) that are
 *                 translated into the AP[2:1], PXN, and UXN hardware fields.
 * @param mem_typ  Memory type index (@ref mem_type_t) written into
 * AttrIndx[2:0] of the leaf descriptor, selecting the MAIR_EL1 attribute byte
 *                 (e.g. @ref device → 0x00 Device-nGnRnE,
 *                       @ref normal → 0xFF Normal WB-RWA Cacheable).
 * @return true  Mapping installed successfully.
 * @return false @p root was NULL, @p v_addr/@p phy_addr were not 4 KB
 * aligned, or allocation of an intermediate table failed.
 */
bool map_page(page_table_t *root, virt_addr v_addr, phy_addr phy_addr,
	      page_permissions_t perms, enum mem_type_t mem_typ);

/**
 * @brief Dump the memory map for debugging.
 * @param root Pointer to the root page table to dump.
 */
void dump_memory_map(page_table_t *root);

/**
 * @brief Sets up the initial identity kernel map
 * @return true on success, false on failure (e.g., if allocation fails).
 */
bool setup_kernel_id_map();

/**
 * @brief Sets up the  kernel map
 * @param mmap Pointer to the memory map structure containing the physical
 * memory layout.
 * @return true on success, false on failure (e.g., if allocation fails).
 */
bool setup_kernel_map(memory_map_t *mmap);

#endif /* PAGE_TABLE_PAGE_TABLE_H */
