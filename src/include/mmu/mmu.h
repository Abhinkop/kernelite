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
 * @enum device_type_t
 * @brief Device memory subtype — encodes bits[3:2] of a Device attribute byte.
 *
 * G  = Gathering   (multiple accesses may be merged into one transaction)
 * R  = Reordering  (accesses to the same device may be reordered)
 * E  = Early write acknowledgement (write can be acknowledged before device)
 */
typedef enum {
	/**< Non-Gathering, Non-Reordering, No early ack */
	DEVICE_nGnRnE = 0b00,

	/**< Non-Gathering, Non-Reordering, Early ack    */
	DEVICE_nGnRE = 0b01,

	/**< Non-Gathering, Reordering,     Early ack    */
	DEVICE_nGRE = 0b10,

	/**< Gathering,     Reordering,     Early ack    */
	DEVICE_GRE = 0b11,
} device_type_t;

/**
 * @brief MAIR attribute byte layout for Device memory.
 *
 * Bits[7:4] must be 0b0000 to mark this slot as Device memory.
 * Bits[3:2] select the device subtype (see @ref device_type_t).
 * Bit[1] is RES0.
 * Bit[0] is the XS bit (FEAT_XS memory-tagging share domain); RES0 otherwise.
 */
typedef struct {
	/**< [0]   XS — FEAT_XS share domain; RES0 if not implemented. */
	uint8_t xs : 1;

	/**< [1]   RES0 */
	uint8_t res0_1 : 1;

	/**< [3:2] Device subtype (see @ref device_type_t). */
	uint8_t type : 2;

	/**< [7:4] Must be 0b0000 to identify as Device memory. */
	uint8_t res0_7_4 : 4;
} __attribute__((packed)) mair_device_attr_t;

/**
 * @brief MAIR attribute byte layout for Normal (cacheable) memory.
 *
 * The byte is split into two 4-bit cache-policy nibbles:
 *   - Inner nibble [3:0]: policy seen by the CPU's own cache hierarchy.
 *   - Outer nibble [7:4]: policy seen by the outer (system-level) cache.
 *
 * Per-nibble encoding:
 *   0b0000 = Device / invalid (only legal for outer nibble to mark Device)
 *   0b0100 = Non-cacheable
 *   0bNWRT = Cacheable: N=Non-transient, W=Write-Back, R=Read-Alloc,
 * T=Write-Alloc
 */
typedef struct {
	/**< [0] Inner Write-Allocate on cache miss. */
	uint8_t inner_write_alloc : 1;

	/**< [1] Inner Read-Allocate on cache miss.  */
	uint8_t inner_read_alloc : 1;

	/**< [2] Inner policy: 1=Write-Back, 0=Write-Through. */
	uint8_t inner_write_back : 1;

	/**< [3] Inner: 1=Non-transient, 0=Transient hint. */
	uint8_t inner_non_transient : 1;

	/**< [4] Outer Write-Allocate on cache miss. */
	uint8_t outer_write_alloc : 1;

	/**< [5] Outer Read-Allocate on cache miss.  */
	uint8_t outer_read_alloc : 1;

	/**< [6] Outer policy: 1=Write-Back, 0=Write-Through. */
	uint8_t outer_write_back : 1;

	/**< [7] Outer: 1=Non-transient, 0=Transient hint. */
	uint8_t outer_non_transient : 1;
} __attribute__((packed)) mair_normal_attr_t;

/**
 * @brief Single MAIR attribute slot (one byte).
 *
 * Overlays a raw byte with typed views for Device or Normal memory.
 * Which union member is active is determined by the upper nibble:
 *   device.res0_7_4 == 0b0000 → Device memory.
 *   Otherwise                 → Normal memory.
 */
typedef struct {
	union {
		/**< Raw attribute byte. */
		uint8_t value;

		/**< Device memory attribute layout. */
		mair_device_attr_t device;

		/**< Normal memory attribute layout. */
		mair_normal_attr_t normal;
	};
} __attribute__((packed)) mair_attr_t;

/**
 * @brief MAIR_EL1 register — 8 independent attribute slots.
 *
 * AttrIndx[2:0] in a leaf page/block descriptor selects which slot applies
 * to that mapping (see @ref mem_type_t for this kernel's slot assignments).
 */
typedef struct {
	union {
		/**< Raw 64-bit register value for MRS/MSR. */
		uint64_t value;

		/**< Per-slot attribute view (attr[0] = bits[7:0]). */
		mair_attr_t attr[8];
	};
} __attribute__((packed)) mair_reg_t;
_Static_assert(sizeof(mair_reg_t) == 8,
	       "FATAL: mair_reg_t must be exactly 8 bytes (64-bit register)");

/**
 * @enum tcr_cacheability_t
 * @brief Inner/Outer cacheability encoding for TCR_EL1 IRGN/ORGN fields.
 *
 * Applies to IRGN0, ORGN0 (TTBR0 walks) and IRGN1, ORGN1 (TTBR1 walks).
 */
typedef enum {
	TCR_CACHE_NON_CACHEABLE = 0b00, /**< Normal, Non-cacheable. */
	TCR_CACHE_WB_RA_WA = 0b01, /**< Normal, Write-Back Read-Alloc
				      Write-Alloc Cacheable. */
	TCR_CACHE_WT_RA_NWA = 0b10, /**< Normal, Write-Through Read-Alloc No
				       Write-Alloc Cacheable. */
	TCR_CACHE_WB_RA_NWA = 0b11, /**< Normal, Write-Back Read-Alloc No
				       Write-Alloc Cacheable. */
} tcr_cacheability_t;

/**
 * @enum tcr_shareability_t
 * @brief Shareability domain encoding for TCR_EL1 SH0/SH1 fields.
 *
 * Applies to SH0 (TTBR0 walks) and SH1 (TTBR1 walks).
 */
typedef enum {
	/**< Non-shareable. */
	TCR_SH_NON_SHAREABLE = 0b00,

	/**< RESERVED — must not be used. */
	TCR_SH_RESERVED = 0b01,

	/**< Outer Shareable. */
	TCR_SH_OUTER_SHAREABLE = 0b10,

	/**< Inner Shareable. */
	TCR_SH_INNER_SHAREABLE = 0b11,
} tcr_shareability_t;

/**
 * @enum tcr_tg0_t
 * @brief Translation granule size for TTBR0_EL1 (TG0 field, bits[15:14]).
 */
typedef enum {
	/**< 4 KB granule. */
	TCR_TG0_4KB = 0b00,

	/**< 64 KB granule. */
	TCR_TG0_64KB = 0b01,

	/**< 16 KB granule. */
	TCR_TG0_16KB = 0b10,
} tcr_tg0_t;

/**
 * @enum tcr_tg1_t
 * @brief Translation granule size for TTBR1_EL1 (TG1 field, bits[31:30]).
 *
 * @note The encoding is different from @ref tcr_tg0_t.
 */
typedef enum {
	/**< 16 KB granule. */
	TCR_TG1_16KB = 0b01,

	/**< 4 KB granule.  */
	TCR_TG1_4KB = 0b10,

	/**< 64 KB granule. */
	TCR_TG1_64KB = 0b11,
} tcr_tg1_t;

/**
 * @enum tcr_ips_t
 * @brief Intermediate Physical Address size (IPS field, bits[34:32]).
 */
typedef enum {
	/**<  32-bit PA (4 GB).  */
	TCR_IPS_32BIT = 0b000,

	/**<  36-bit PA (64 GB). */
	TCR_IPS_36BIT = 0b001,

	/**<  40-bit PA (1 TB).  */
	TCR_IPS_40BIT = 0b010,

	/**<  44-bit PA (16 TB). */
	TCR_IPS_44BIT = 0b011,

	/**<  48-bit PA (256 TB). */
	TCR_IPS_48BIT = 0b100,

	/**<  52-bit PA (4 PB, FEAT_LPA). */
	TCR_IPS_52BIT = 0b101,
} tcr_ips_t;

/**
 * @brief TCR_EL1 — Translation Control Register, EL1.
 *
 * Controls the stage 1 translation regime for EL0 and EL1.
 * TTBR0_EL1 governs the lower VA range [0, 2^(64-T0SZ)).
 * TTBR1_EL1 governs the upper VA range [2^(64-T1SZ), 2^64).
 *
 * Reference: ARMv8/ARMv9 ARM, section D19.2.131 TCR_EL1.
 */
typedef struct __attribute__((packed)) {
	union {
		uint64_t value; /**< Raw 64-bit register value for MRS/MSR. */
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
			 * walks (see @ref tcr_cacheability_t). */
			uint64_t irgn0 : 2;
			/** [11:10] ORGN0 — Outer cacheability of TTBR0 table
			 * walks (see @ref tcr_cacheability_t). */
			uint64_t orgn0 : 2;
			/** [13:12] SH0 — Shareability of TTBR0 table walks (see
			 * @ref tcr_shareability_t). */
			uint64_t sh0 : 2;
			/** [15:14] TG0 — Granule size for TTBR0 (see @ref
			 * tcr_tg0_t). */
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
			 * walks (see @ref tcr_cacheability_t). */
			uint64_t irgn1 : 2;
			/** [27:26] ORGN1 — Outer cacheability of TTBR1 table
			 * walks (see @ref tcr_cacheability_t). */
			uint64_t orgn1 : 2;
			/** [29:28] SH1 — Shareability of TTBR1 table walks (see
			 * @ref tcr_shareability_t). */
			uint64_t sh1 : 2;
			/** [31:30] TG1 — Granule size for TTBR1 (see @ref
			 * tcr_tg1_t). */
			uint64_t tg1 : 2;

			/* ── Physical address size ──────────────────────────
			 */
			/** [34:32] IPS — Intermediate PA size (see @ref
			 * tcr_ips_t). */
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
 * @brief SCTLR_EL1 — System Control Register, EL1.
 *
 * Controls the EL1&0 execution environment: MMU, caches, alignment checks,
 * pointer authentication, branch target identification, and memory tagging.
 *
 * Reference: ARMv8/ARMv9 ARM DDI 0601, section SCTLR_EL1.
 */
typedef struct __attribute__((packed)) {
	union {
		uint64_t value; /**< Raw 64-bit register value for MRS/MSR. */
		struct __attribute__((packed)) {
			/* ── Core MMU / Cache / Alignment ─────────────────────
			 */
			/** [0]  M — MMU enable for EL1&0 stage 1 translation.
			 * 0=off, 1=on. */
			uint64_t m : 1;
			/** [1]  A — Alignment fault enable. 1=fault on
			 * unaligned access. */
			uint64_t a : 1;
			/** [2]  C — Data cache enable. 0=off, 1=on. */
			uint64_t c : 1;
			/** [3]  SA — EL1 stack pointer alignment check enable.
			 */
			uint64_t sa : 1;
			/** [4]  SA0 — EL0 stack pointer alignment check enable.
			 */
			uint64_t sa0 : 1;
			/** [5]  CP15BEN — CP15 barrier instruction enable
			 * (FEAT_CP15BEN). */
			uint64_t cp15ben : 1;
			/** [6]  nAA — Non-aligned access enable (FEAT_LSE2).
			 *         1=hardware handles unaligned accesses at
			 * EL1/EL0. */
			uint64_t naa : 1;
			/** [7]  ITD — IT instruction disable for T32 at EL0. */
			uint64_t itd : 1;
			/** [8]  SED — SETEND instruction disable. */
			uint64_t sed : 1;
			/** [9]  UMA — User Mask Access: 1=EL0 may read/write
			 * DAIF. */
			uint64_t uma : 1;
			/** [10] EnRCTX — EL0 access to FEAT_RCPC instructions
			 * enable. */
			uint64_t enrctx : 1;
			/** [11] EOS — Exception exit is a context
			 * synchronization event (FEAT_ExS). */
			uint64_t eos : 1;
			/** [12] I — Instruction cache enable. 0=off, 1=on. */
			uint64_t i : 1;
			/** [13] EnDB — Pointer authentication using APDBKey_EL1
			 * enable (FEAT_PAuth). */
			uint64_t endb : 1;
			/** [14] DZE — EL0 access to DC ZVA instruction enable.
			 */
			uint64_t dze : 1;
			/** [15] UCT — EL0 access to CTR_EL0 enable. */
			uint64_t uct : 1;
			/** [16] nTWI — Do not trap EL0 WFI instructions to EL1.
			 */
			uint64_t ntwi : 1;
			/** [17] RES0. */
			uint64_t res0_17 : 1;
			/** [18] nTWE — Do not trap EL0 WFE instructions to EL1.
			 */
			uint64_t ntwe : 1;
			/** [19] WXN — Write permission implies Execute-Never at
			 * EL1. */
			uint64_t wxn : 1;
			/** [20] TSCXT — Trap EL0 access to SCXTNUM_EL0
			 * (FEAT_CSV2_2). */
			uint64_t tscxt : 1;
			/** [21] IESB — Implicit Error Synchronization Barrier
			 * enable (FEAT_IESB). */
			uint64_t iesb : 1;
			/** [22] EIS — Exception entry is a context
			 * synchronization event (FEAT_ExS). */
			uint64_t eis : 1;
			/** [23] SPAN — Set PAN on exception entry to EL1
			 * (FEAT_PAN). 0=PSTATE.PAN set to 1 on entry,
			 * 1=PSTATE.PAN unchanged. */
			uint64_t span : 1;
			/** [24] E0E — EL0 data accesses are big-endian. */
			uint64_t e0e : 1;
			/** [25] EE — Exception endianness. 0=little-endian,
			 * 1=big-endian. */
			uint64_t ee : 1;
			/** [26] UCI — EL0 cache maintenance instructions (DC
			 * CVAU, IC IVAU, etc.) enable. */
			uint64_t uci : 1;
			/** [27] EnDA — Pointer authentication using APDAKey_EL1
			 * enable (FEAT_PAuth). */
			uint64_t enda : 1;
			/** [28] nTLSMD — No trap LDM/STM/LDP/STP to
			 * Device-nGRE/nGnRE/nGnRnE (FEAT_LSMAOC). */
			uint64_t ntlsmd : 1;
			/** [29] LSMAOE — LDM/STM atomicity and ordering enable
			 * (FEAT_LSMAOC). */
			uint64_t lsmaoe : 1;
			/** [30] EnIB — Pointer authentication using APIBKey_EL1
			 * enable (FEAT_PAuth). */
			uint64_t enib : 1;
			/** [31] EnIA — Pointer authentication using APIAKey_EL1
			 * enable (FEAT_PAuth). */
			uint64_t enia : 1;

			/* ── Upper feature controls
			 * ──────────────────────────── */
			/** [34:32] RES0. */
			uint64_t res0_34_32 : 3;
			/** [35] BT0 — PAC Branch Type Compatibility at EL0
			 * (FEAT_BTI). 1=PACIASP/PACIBSP not compatible with
			 * PSTATE.BTYPE==0b11 at EL0. */
			uint64_t bt0 : 1;
			/** [36] BT1 — PAC Branch Type Compatibility at EL1
			 * (FEAT_BTI). 1=PACIASP/PACIBSP not compatible with
			 * PSTATE.BTYPE==0b11 at EL1. */
			uint64_t bt1 : 1;
			/** [37] ITFSB — Implicit Tag Fault Synchronization
			 * Barrier on EL1 entry (FEAT_MTE2). */
			uint64_t itfsb : 1;
			/** [39:38] TCF0 — EL0 Tag Check Fault mode (FEAT_MTE2).
			 *           00=no effect, 01=sync exception, 10=async
			 * accumulated, 11=sync reads/async writes (FEAT_MTE3).
			 */
			uint64_t tcf0 : 2;
			/** [41:40] TCF — EL1 Tag Check Fault mode (FEAT_MTE2).
			 *           00=no effect, 01=sync exception, 10=async
			 * accumulated, 11=sync reads/async writes (FEAT_MTE3).
			 */
			uint64_t tcf : 2;
			/** [42] ATA0 — EL0 Allocation Tag Access enable
			 * (FEAT_MTE2). */
			uint64_t ata0 : 1;
			/** [43] ATA — EL1 Allocation Tag Access enable
			 * (FEAT_MTE2). */
			uint64_t ata : 1;
			/** [44] DSSBS — Default PSTATE.SSBS value on exception
			 * entry to EL1 (FEAT_SSBS). */
			uint64_t dssbs : 1;
			/** [45] TWEDEn — TWE Delay enable (FEAT_TWED). */
			uint64_t tweden : 1;
			/** [49:46] TWEDEL — TWE Delay value; trap delay =
			 * 2^(TWEDEL+8) cycles (FEAT_TWED). */
			uint64_t twedel : 4;
			/** [53:50] RES0. */
			uint64_t res0_53_50 : 4;
			/** [54] EnASR — Trap EL0 ST64BV instruction to EL1
			 * (FEAT_LS64). */
			uint64_t enasr : 1;
			/** [55] EnAS0 — Trap EL0 ST64BV0 instruction to EL1
			 * (FEAT_LS64). */
			uint64_t enas0 : 1;
			/** [56] EnALS — Trap EL0 LD64B/ST64B instructions to
			 * EL1 (FEAT_LS64). */
			uint64_t enals : 1;
			/** [57] EPAN — Enhanced Privileged Access Never
			 * (FEAT_PAN3). 1=EL1 data access to EL0-accessible
			 * pages generates Permission fault. */
			uint64_t epan : 1;
			/** [63:58] RES0. */
			uint64_t res0_63_58 : 6;
		};
	};
} sctlr_el1_t;
_Static_assert(sizeof(sctlr_el1_t) == 8,
	       "FATAL: sctlr_el1_t must be exactly 8 bytes (64-bit register)");

/**
 * @brief Configure system registers and enable the AArch64 EL1 MMU.
 *
 * Performs the full MMU bring-up sequence in order:
 *  1. Programs MAIR_EL1 with the kernel's two memory attribute slots
 *     (slot 0 = Device-nGnRnE, slot 1 = Normal WB-RWA Cacheable).
 *  2. Configures TCR_EL1 for a 48-bit VA space (T0SZ=16), 4 KB granule,
 *     inner/outer write-back cacheable table walks, inner-shareable domain,
 *     40-bit IPS, and EPD1=1 to disable TTBR1 walks.
 *  3. Writes the physical address of @p root into TTBR0_EL1.
 *  4. Enables the MMU (M), D-cache (C), and I-cache (I) in SCTLR_EL1
 *
 * @param root  Pointer to the root (L0) page table whose physical address
 *              is loaded into TTBR0_EL1.  Must not be NULL.
 * @return true   MMU enabled successfully.
 * @return false  @p root was NULL.
 */
bool enable_mmu(page_table_t *root);

#endif /* MMU_MMU_H */
