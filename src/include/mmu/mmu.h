/**
 * @file mmu.h
 * @brief Memory Management Unit utilities.
 *
 * Provides architecture-specific mmu setup.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-25
 */

#ifndef MMU_MMU_H
#define MMU_MMU_H

#include "page_table/page_table.h"

#include <stdint.h>

/**
 * @brief TCR_EL1 — Translation Control Register, EL1.
 *
 * Controls the stage 1 translation regime for EL0 and EL1.
 * TTBR0_EL1 governs the lower VA range [0, 2^(64-T0SZ)).
 * TTBR1_EL1 governs the upper VA range [2^(64-T1SZ), 2^64).
 *
 * Reference: ARMv8/ARMv9 ARM, section D19.2.131 TCR_EL1.
 */
typedef struct __attribute__((packed)) tcr_reg_t {
	union {
		/** Raw 64-bit register value for MRS/MSR. */
		uint64_t value;

		/** @brief Fields of the TCR_EL1 register. */
		struct __attribute__((packed)) {
			/* ── TTBR0 region ───────────────────────────────────
			 */
			/** [5:0]   T0SZ — VA size = 2^(64-T0SZ); 16 = 48-bit
			 * space. */
			uint64_t t0sz : 6;

			/** [6]     RES0. */
			uint64_t res0_6 : 1;

			/** [7]     EPD0 — 0=walk TTBR0, 1=fault on TTBR0 miss.
			 */
			uint64_t epd0 : 1;

			/** [9:8]   IRGN0 — Inner cacheability of TTBR0 table
			 * walks (see @ref tcr_cacheability). */
			uint64_t irgn0 : 2;

			/** [11:10] ORGN0 — Outer cacheability of TTBR0 table
			 * walks (see @ref tcr_cacheability). */
			uint64_t orgn0 : 2;

			/** [13:12] SH0 — Shareability of TTBR0 table walks (see
			 * @ref tcr_shareability). */
			uint64_t sh0 : 2;

			/** [15:14] TG0 — Granule size for TTBR0 (see @ref
			 * tcr_tg0). */
			uint64_t tg0 : 2;

			/* ── TTBR1 region ───────────────────────────────────
			 */
			/** [21:16] T1SZ — VA size = 2^(64-T1SZ) for TTBR1
			 * region. */
			uint64_t t1sz : 6;

			/** [22]    A1 — ASID from TTBR0_EL1 (0) or TTBR1_EL1
			 * (1). */
			uint64_t a1 : 1;

			/** [23]    EPD1 — 0=walk TTBR1, 1=fault on TTBR1 miss.
			 */
			uint64_t epd1 : 1;

			/** [25:24] IRGN1 — Inner cacheability of TTBR1 table
			 * walks (see @ref tcr_cacheability). */
			uint64_t irgn1 : 2;

			/** [27:26] ORGN1 — Outer cacheability of TTBR1 table
			 * walks (see @ref tcr_cacheability). */
			uint64_t orgn1 : 2;

			/** [29:28] SH1 — Shareability of TTBR1 table walks (see
			 * @ref tcr_shareability). */
			uint64_t sh1 : 2;

			/** [31:30] TG1 — Granule size for TTBR1 (see @ref
			 * tcr_tg1). */
			uint64_t tg1 : 2;

			/* ── Physical address size ──────────────────────────
			 */
			/** [34:32] IPS — Intermediate PA size (see @ref
			 * tcr_ips). */
			uint64_t ips : 3;

			/** [35]    RES0. */
			uint64_t res0_35 : 1;

			/** [36]    AS — ASID size: 0=8-bit, 1=16-bit.
			 *           RES0 if the implementation only supports
			 * 8-bit ASIDs. */
			uint64_t as : 1;

			/* ── Top-byte ignore ────────────────────────────────
			 */
			/** [37]    TBI0 — Top byte of TTBR0 addresses ignored
			 * for address matching. */
			uint64_t tbi0 : 1;

			/** [38]    TBI1 — Top byte of TTBR1 addresses ignored
			 * for address matching. */
			uint64_t tbi1 : 1;

			/* ── Hardware Access / Dirty (FEAT_HAF / FEAT_HAFDBS)
			 */
			/** [39]    HA — Hardware Access Flag update enable
			 * (FEAT_HAF). */
			uint64_t ha : 1;

			/** [40]    HD — Hardware Dirty state update enable
			 * (FEAT_HAFDBS). Effective only when HA == 1. */
			uint64_t hd : 1;

			/* ── Hierarchical Permission Disable (FEAT_HPDS) ─────
			 */
			/** [41]    HPD0 — Disable APTable/PXNTable/UXNTable for
			 * TTBR0 walks. */
			uint64_t hpd0 : 1;

			/** [42]    HPD1 — Disable APTable/PXNTable/UXNTable for
			 * TTBR1 walks. */
			uint64_t hpd1 : 1;

			/* ── Hardware Use bits (FEAT_HPDS2) ─────────────────
			 */
			/** [43]    HWU059 — HW use of descriptor bit[59] for
			 * TTBR0 (FEAT_HPDS2). */
			uint64_t hwu059 : 1;

			/** [44]    HWU060 — HW use of descriptor bit[60] for
			 * TTBR0 (FEAT_HPDS2). */
			uint64_t hwu060 : 1;

			/** [45]    HWU061 — HW use of descriptor bit[61] for
			 * TTBR0 (FEAT_HPDS2). */
			uint64_t hwu061 : 1;

			/** [46]    HWU062 — HW use of descriptor bit[62] for
			 * TTBR0 (FEAT_HPDS2). */
			uint64_t hwu062 : 1;

			/** [47]    HWU159 — HW use of descriptor bit[59] for
			 * TTBR1 (FEAT_HPDS2). */
			uint64_t hwu159 : 1;

			/** [48]    HWU160 — HW use of descriptor bit[60] for
			 * TTBR1 (FEAT_HPDS2). */
			uint64_t hwu160 : 1;

			/** [49]    HWU161 — HW use of descriptor bit[61] for
			 * TTBR1 (FEAT_HPDS2). */
			uint64_t hwu161 : 1;

			/** [50]    HWU162 — HW use of descriptor bit[62] for
			 * TTBR1 (FEAT_HPDS2). */
			uint64_t hwu162 : 1;

			/* ── Top-byte ignore for data only (FEAT_PAuth) ─────
			 */
			/** [51]    TBID0 — TBI0 applies to data accesses only
			 * for TTBR0. */
			uint64_t tbid0 : 1;

			/** [52]    TBID1 — TBI1 applies to data accesses only
			 * for TTBR1. */
			uint64_t tbid1 : 1;

			/* ── Non-Fault Disable (FEAT_SVE) ───────────────────
			 */
			/** [53]    NFD0 — Non-fault walk disable for TTBR0
			 * (FEAT_SVE). */
			uint64_t nfd0 : 1;

			/** [54]    NFD1 — Non-fault walk disable for TTBR1
			 * (FEAT_SVE). */
			uint64_t nfd1 : 1;

			/* ── EL0 fault on TTBR access (FEAT_E0PD) ───────────
			 */
			/** [55]    E0PD0 — EL0 TTBR0 access generates level 0
			 * Translation Fault. */
			uint64_t e0pd0 : 1;

			/** [56]    E0PD1 — EL0 TTBR1 access generates level 0
			 * Translation Fault. */
			uint64_t e0pd1 : 1;

			/* ── Tag Check Mask (FEAT_MTE2) ──────────────────────
			 */
			/** [57]    TCMA0 — Unchecked accesses when
			 * addr[59:55]=0b00000 (FEAT_MTE2). */
			uint64_t tcma0 : 1;

			/** [58]    TCMA1 — Unchecked accesses when
			 * addr[59:55]=0b11111 (FEAT_MTE2). */
			uint64_t tcma1 : 1;

			/* ── 52-bit PA (FEAT_LPA2) ──────────────────────────
			 */
			/** [59]    DS — Enable 52-bit OA in 4KB/16KB granule
			 * descriptors (FEAT_LPA2). RES0 for 64KB granule. */
			uint64_t ds : 1;

			/* ── Extended MTE canonical tagging
			 * (FEAT_MTE_NO_ADDRESS_TAGS / FEAT_MTE_CANONICAL_TAGS)
			 */
			/** [60]    MTX0 — Canonical address tagging for TTBR0
			 * region. */
			uint64_t mtx0 : 1;

			/** [61]    MTX1 — Canonical address tagging for TTBR1
			 * region. */
			uint64_t mtx1 : 1;

			/** [63:62] RES0. */
			uint64_t res0_63_62 : 2;
		};
	};
} tcr_reg_t;
_Static_assert(sizeof(tcr_reg_t) == 8,
	       "FATAL: tcr_reg_t must be exactly 8 bytes (64-bit register)");

/**
 * @brief Configure system registers and enable the AArch64 EL1 MMU.
 *
 * Performs the full MMU bring-up sequence in order:
 *  1. Programs MAIR_EL1 with the kernel's two memory attribute slots
 *     (slot 0 = Device-nGnRnE, slot 1 = Normal WB-RWA Cacheable).
 *  2. Configures TCR_EL1 for a 48-bit VA space (T0SZ=16), 4 KB granule,
 *     inner/outer write-back cacheable table walks, inner-shareable domain,
 *     and 40-bit IPS
 *  3. Writes the physical address of @p id_map_root into TTBR0_EL1.
 *  4. Writes the physical address of @p kernel_map_root into TTBR1_EL1.
 *  5. Enables the MMU (M), D-cache (C), and I-cache (I) in SCTLR_EL1
 *
 * @param id_map_root  Pointer to the id_map_root (L0) page table whose physical
 * address is loaded into TTBR0_EL1.  Must not be NULL.
 * @param kernel_map_root  Pointer to the kernel_map_root (L1) page table whose
 * physical address is loaded into TTBR1_EL1.  Must not be NULL.
 * @return true   MMU enabled successfully.
 * @return false  @p id_map_root or @p kernel_map_root was NULL.
 */
bool enable_mmu(page_table_t *id_map_root, page_table_t *kernel_map_root);

#endif /* MMU_MMU_H */
