/**
 * @file mmu.c
 * @brief Implementation of Memory Management Unit utilities.
 *
 * Provides architecture-specific mmu setup.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-25
 */

#include "mmu/mmu.h"

#include "asm/asm_helper.h"
#include "mem_layout/mem_layout.h"
#include "utils/kprintf.h"

#include <stdint.h>

/**
 * @brief Device memory subtype — encodes bits[3:2] of a Device attribute byte.
 *
 * G  = Gathering   (multiple accesses may be merged into one transaction)
 * R  = Reordering  (accesses to the same device may be reordered)
 * E  = Early write acknowledgement (write can be acknowledged before device)
 */
enum device_type {
	/** Non-Gathering, Non-Reordering, No early ack */
	DEVICE_nGnRnE = 0b00,

	/** Non-Gathering, Non-Reordering, Early ack    */
	DEVICE_nGnRE = 0b01,

	/** Non-Gathering, Reordering,     Early ack    */
	DEVICE_nGRE = 0b10,

	/** Gathering,     Reordering,     Early ack    */
	DEVICE_GRE = 0b11,
};

/**
 * @brief MAIR attribute byte layout for Device memory.
 *
 * Bits[7:4] must be 0b0000 to mark this slot as Device memory.
 * Bits[3:2] select the device subtype (see @ref device_type).
 * Bit[1] is RES0.
 * Bit[0] is the XS bit (FEAT_XS memory-tagging share domain); RES0 otherwise.
 */
typedef struct __attribute__((packed)) mair_device_attr_t {
	/** [0]   XS — FEAT_XS share domain; RES0 if not implemented. */
	uint8_t xs : 1;

	/** [1]   RES0 */
	uint8_t res0_1 : 1;

	/** [3:2] Device subtype (see @ref device_type). */
	uint8_t type : 2;

	/** [7:4] Must be 0b0000 to identify as Device memory. */
	uint8_t res0_7_4 : 4;
} mair_device_attr_t;

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
typedef struct __attribute__((packed)) mair_normal_attr_t {
	/** [0] Inner Write-Allocate on cache miss. */
	uint8_t inner_write_alloc : 1;

	/** [1] Inner Read-Allocate on cache miss.  */
	uint8_t inner_read_alloc : 1;

	/** [2] Inner policy: 1=Write-Back, 0=Write-Through. */
	uint8_t inner_write_back : 1;

	/** [3] Inner: 1=Non-transient, 0=Transient hint. */
	uint8_t inner_non_transient : 1;

	/** [4] Outer Write-Allocate on cache miss. */
	uint8_t outer_write_alloc : 1;

	/** [5] Outer Read-Allocate on cache miss.  */
	uint8_t outer_read_alloc : 1;

	/** [6] Outer policy: 1=Write-Back, 0=Write-Through. */
	uint8_t outer_write_back : 1;

	/** [7] Outer: 1=Non-transient, 0=Transient hint. */
	uint8_t outer_non_transient : 1;
} mair_normal_attr_t;

/**
 * @brief Single MAIR attribute slot (one byte).
 *
 * Overlays a raw byte with typed views for Device or Normal memory.
 * Which union member is active is determined by the upper nibble:
 *   device.res0_7_4 == 0b0000 → Device memory.
 *   Otherwise                 → Normal memory.
 */
typedef struct __attribute__((packed)) mair_attr_t {
	union {
		/** Raw attribute byte. */
		uint8_t value;

		/** Device memory attribute layout. */
		mair_device_attr_t device;

		/** Normal memory attribute layout. */
		mair_normal_attr_t normal;
	};
} mair_attr_t;

/**
 * @brief MAIR_EL1 register — 8 independent attribute slots.
 *
 * AttrIndx[2:0] in a leaf page/block descriptor selects which slot applies
 * to that mapping (see @ref mem_type_t for this kernel's slot assignments).
 */
typedef struct __attribute__((packed)) mair_reg_t {
	union {
		/** Raw 64-bit register value for MRS/MSR. */
		uint64_t value;

		/** Per-slot attribute view (attr[0] = bits[7:0]). */
		mair_attr_t attr[8];
	};
} mair_reg_t;
_Static_assert(sizeof(mair_reg_t) == 8,
	       "FATAL: mair_reg_t must be exactly 8 bytes (64-bit register)");

/**
 * @brief Inner/Outer cacheability encoding for TCR_EL1 IRGN/ORGN fields.
 *
 * Applies to IRGN0, ORGN0 (TTBR0 walks) and IRGN1, ORGN1 (TTBR1 walks).
 */
enum tcr_cacheability {
	TCR_CACHE_NON_CACHEABLE = 0b00, /**< Normal, Non-cacheable. */
	TCR_CACHE_WB_RA_WA = 0b01, /**< Normal, Write-Back Read-Alloc
				      Write-Alloc Cacheable. */
	TCR_CACHE_WT_RA_NWA = 0b10, /**< Normal, Write-Through Read-Alloc No
				       Write-Alloc Cacheable. */
	TCR_CACHE_WB_RA_NWA = 0b11, /**< Normal, Write-Back Read-Alloc No
				       Write-Alloc Cacheable. */
};

/**
 * @brief Shareability domain encoding for TCR_EL1 SH0/SH1 fields.
 *
 * Applies to SH0 (TTBR0 walks) and SH1 (TTBR1 walks).
 */
enum tcr_shareability {
	TCR_SH_NON_SHAREABLE = 0b00, /**< Non-Shareable. */
	TCR_SH_RESERVED = 0b01, /**< RESERVED — must not be used. */
	TCR_SH_OUTER_SHAREABLE = 0b10, /**< Outer Shareable. */
	TCR_SH_INNER_SHAREABLE = 0b11, /**< Inner Shareable. */
};

/**
 * @brief Translation granule size for TTBR0_EL1 (TG0 field, bits[15:14]).
 */
enum tcr_tg0 {
	TCR_TG0_4KB = 0b00, /**< 4 KB granule. */
	TCR_TG0_64KB = 0b01, /**< 64 KB granule. */
	TCR_TG0_16KB = 0b10, /**< 16 KB granule. */
};

/**
 * @brief Translation granule size for TTBR1_EL1 (TG1 field, bits[31:30]).
 *
 * @note The encoding is different from @ref tcr_tg0.
 */
enum tcr_tg1 {
	TCR_TG1_16KB = 0b01, /**< 16 KB granule. */
	TCR_TG1_4KB = 0b10, /**< 4 KB granule.  */
	TCR_TG1_64KB = 0b11, /**< 64 KB granule. */
};

/**
 * @brief Intermediate Physical Address size (IPS field, bits[34:32]).
 */
enum tcr_ips {
	TCR_IPS_32BIT = 0b000, /**<  32-bit PA (4 GB).  */
	TCR_IPS_36BIT = 0b001, /**<  36-bit PA (64 GB). */
	TCR_IPS_40BIT = 0b010, /**<  40-bit PA (1 TB).  */
	TCR_IPS_44BIT = 0b011, /**<  44-bit PA (16 TB). */
	TCR_IPS_48BIT = 0b100, /**<  48-bit PA (256 TB). */
	TCR_IPS_52BIT = 0b101, /**<  52-bit PA (4 PB, FEAT_LPA). */
};

/**
 * @brief SCTLR_EL1 — System Control Register, EL1.
 *
 * Controls the EL1&0 execution environment: MMU, caches, alignment checks,
 * pointer authentication, branch target identification, and memory tagging.
 *
 * Reference: ARMv8/ARMv9 ARM DDI 0601, section SCTLR_EL1.
 */
typedef struct __attribute__((packed)) sctlr_el1_t {
	union {
		/** Raw 64-bit register value for MRS/MSR. */
		uint64_t value;
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

			/** [49:46] TWEDEL — TWE Delay value;trap delay =
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

bool enable_mmu(page_table_t *id_map_root, page_table_t *kernel_map_root)
{
	if (!id_map_root || !kernel_map_root) {
		return false;
	}

	// Setup MAIR (Memory Attribute Indirection Register)
	mair_reg_t mair;
	mair.value = 0;

	// Attribute 0: Device-nGnRnE (standard for MMIO/UART/Peripherals)
	mair.attr[0].device.type = DEVICE_nGnRnE;
	mair.attr[0].device.xs = 0;

	// Attribute 1: Normal Memory, Outer/Inner Write-Back Non-transient
	mair.attr[1].normal.outer_non_transient = 1;
	mair.attr[1].normal.outer_write_back = 1;
	mair.attr[1].normal.outer_read_alloc = 1;
	mair.attr[1].normal.outer_write_alloc = 1;
	mair.attr[1].normal.inner_non_transient = 1;
	mair.attr[1].normal.inner_write_back = 1;
	mair.attr[1].normal.inner_read_alloc = 1;
	mair.attr[1].normal.inner_write_alloc = 1;
	WRITE_SYS_REG(mair_el1, mair.value);

	// Setup TCR (Translation Control Register)
	tcr_reg_t tcr;
	tcr.value = 0;

	// TTBR0 (low VA, identity map)
	// T0SZ: 48-bit VA space
	tcr.t0sz = (uint64_t)(64 - 48);
	// IRGN0: Inner Write-Back Cacheable
	tcr.irgn0 = TCR_CACHE_WB_RA_NWA;
	// ORGN0: Outer Write-Back Cacheable
	tcr.orgn0 = TCR_CACHE_WB_RA_NWA;
	// SH0: Inner Shareable boundary
	tcr.sh0 = TCR_SH_INNER_SHAREABLE;
	// TG0: 4Kb
	tcr.tg0 = TCR_TG0_4KB;

	// TTBR1 (high VA, kernel map) — add these
	// T1SZ: 48-bit VA space
	tcr.t1sz = (uint64_t)(64 - 48);
	// IRGN1: Inner Write-Back Cacheable
	tcr.irgn1 = TCR_CACHE_WB_RA_NWA;
	// ORGN1: Outer Write-Back Cacheable
	tcr.orgn1 = TCR_CACHE_WB_RA_NWA;
	// SH1: Inner Shareable boundary
	tcr.sh1 = TCR_SH_INNER_SHAREABLE;
	// TG1: 4Kb
	tcr.tg1 = TCR_TG1_4KB;

	// IPS: 40-bit PA limits
	tcr.ips = TCR_IPS_40BIT;
	// EPD1: Enable TTBR1 walking paths
	tcr.epd1 = 0;

	WRITE_SYS_REG(tcr_el1, tcr.value);
	asm volatile("isb");

	// Set ttbr0
	phy_addr root_pa = va_to_pa((virt_addr)id_map_root);
	WRITE_SYS_REG(ttbr0_el1, root_pa);

	// Set ttbr1
	root_pa = va_to_pa((virt_addr)kernel_map_root);
	WRITE_SYS_REG(ttbr1_el1, root_pa);

	// Ensure the TTBR writes are visible before enabling the MMU
	asm volatile("isb");

	// Fetch and configure the System Control Register (SCTLR_EL1)
	sctlr_el1_t sctlr1;
	READ_SYS_REG(sctlr_el1, sctlr1.value);
	sctlr1.a = 0; // Clear A (Alignment check faulting)
	sctlr1.ee = 0; // Little Endian enforcement
	sctlr1.m = 1; // Enable MMU
	sctlr1.c = 1; // Enable Data Cache
	sctlr1.i = 1; // Enable Instruction Cache

	// Atomic Context Sync Execution
	asm volatile(
		// Ensure all preceding memory config writes settle complete
		"dsb sy\n\t"
		// Write modified flags to system control register
		"msr sctlr_el1, %0\n\t"
		// Flash local CPU pipeline, enforcing instant virtual mapping
		// lookup rules
		"isb\n\t"
		:
		: "r"(sctlr1.value)
		: "memory");

	kprintf("MMU: Successfully enabled and running in Virtual Memory!\n");
	return true;
}
