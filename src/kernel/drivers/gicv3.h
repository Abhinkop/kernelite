/**
 * @file gicv3.h
 * @brief ARM GICv3 interrupt controller definitions.
 *
 * Declares the public GICv3 initialization and management API used by the
 * kernel's interrupt controller unit (ICU) layer.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-06-14
 */

#ifndef DRIVERS_GICV3_H
#define DRIVERS_GICV3_H

#include "mem_layout/mem_layout.h"
#include "icu/icu.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/** @brief Number of GICR_IPRIORITYR registers per redistributor (8 regs x
 *  4 INTIDs each = 32 SGI/PPI priority fields). */
#define GICR_IPRIORITYR_COUNT 8U

/* ── Distributor ──────────────────────────────────────────────────────── */
/**
 * @brief GICv3 Distributor Control Register, GICD_CTLR (0x0000).
 *
 * Bit layout per Arm IHI 0069. On QEMU virt the GIC runs with a single
 * security state (DS reads as 1, RAO/WI), in which case ARE_S/ARE_NS/
 * EnableGrp1S are not meaningful and only EnableGrp0/EnableGrp1NS/DS/RWP
 * are relevant.
 */
typedef union gicd_ctlr_t {
	/** @brief Raw 32-bit register value. */
	uint32_t raw;
	struct __attribute__((packed)) {
		/** @brief Enables Group 0 interrupts. */
		bool enable_grp0 : 1;

		/** @brief Enables Non-secure Group 1 interrupts. */
		bool enable_grp1ns : 1;

		/** @brief Enables Secure Group 1 interrupts. RES0 if are_s ==
		 * 0. */
		bool enable_grp1s : 1;

		/** @brief Reserved. */
		bool res0 : 1;

		/** @brief Affinity Routing Enable, Secure state. */
		bool are_s : 1;

		/** @brief Affinity Routing Enable, Non-secure state. */
		bool are_ns : 1;

		/** @brief Disable Security. RAO/WI on implementations with a
		 * single security state. */
		bool ds : 1;

		/** @brief Enable 1-of-N Wakeup Functionality. */
		bool e1nwf : 1;

		/** @brief Reserved (includes the nASSGIreq field). */
		uint32_t res1 : 23;

		/** @brief Register Write Pending. Read-only; poll until 0 after
		 * writes this field tracks. */
		bool rwp : 1;
	};
} gicd_ctlr_t;
_Static_assert(sizeof(gicd_ctlr_t) == 4, "GICD_CTLR must be 32 bits");

/**
 * @brief GICv3 Distributor Type Register, GICD_TYPER (0x0004, RO).
 *
 * Bit layout per Arm IHI 0069. Fields relating to extended SPIs, LPIs, and
 * the 1-of-N/range-selector extensions are decoded for completeness but are
 * not exercised by this driver, which targets a single PE without an ITS.
 */
typedef union gicd_typer_t {
	/** @brief Raw 32-bit register value. */
	uint32_t raw;
	struct __attribute__((packed)) {
		/** @brief (ITLinesNumber + 1) * 32 - 1 = highest SPI INTID
		 * supported. */
		uint32_t itlinesnumber : 5;

		/** @brief Number of PEs visible with affinity routing disabled;
		 * unused once ARE is set. */
		uint32_t cpunumber : 3;

		/** @brief 1 if the extended SPI range is implemented. */
		bool espi : 1;

		/** @brief 1 if Non-maskable Interrupt priorities are supported.
		 */
		bool nmi : 1;

		/** @brief 1 if the GIC implements two security states. Reads as
		 * 0 when GICD_CTLR.DS == 1. */
		bool security_extn : 1;

		/** @brief Number of LPIs supported. 0: determined by idbits
		 * instead. Otherwise, 2^(num_lpis+1) LPIs. */
		uint32_t num_lpis : 5;

		/** @brief 1 if message-based SPIs via
		 * GICD_SETSPI_NSR/CLRSPI_NSR are supported. */
		bool mbis : 1;

		/** @brief 1 if LPIs are supported. */
		bool lpis : 1;

		/** @brief 1 if direct injection of virtual LPIs is supported.
		 */
		bool dvis : 1;

		/** @brief Number of physical interrupt identifier bits
		 * supported, minus 1. */
		uint32_t idbits : 5;

		/** @brief 1 if non-zero values of affinity level 3 are
		 * supported. */
		bool a3v : 1;

		/** @brief 1 if 1-of-N SPI interrupt targeting is not supported.
		 */
		bool no1n : 1;

		/** @brief 1 if the Range Selector field is supported in
		 * GICD_IROUTER<n> and ICC_SGI*R_EL1. */
		bool rss : 1;

		/** @brief If espi == 1, (espi_range + 1) * 32 extended SPIs are
		 * supported beyond INTID 4095. */
		uint32_t espi_range : 5;
	};
} gicd_typer_t;
_Static_assert(sizeof(gicd_typer_t) == 4, "GICD_TYPER must be 32 bits");

/**
 * @brief Distributor Implementer Identification Register, GICD_IIDR (0x0008,
 * RO).
 *
 * Identifies the implementer and product of the Distributor. Useful as a
 * diagnostic during bring-up (e.g. confirming an Arm GICv3 implementation),
 * but not required for normal operation.
 */
typedef union gicd_iidr_t {
	/** @brief Raw 32-bit register value. */
	uint32_t raw;
	struct __attribute__((packed)) {
		/** @brief JEP106 implementer code. Bits 11:8 are the
		 * continuation count (0x4 for Arm), bit 7 is 0, bits 6:0 are
		 * the identity code. */
		uint32_t implementer : 12;

		/** @brief IMPLEMENTATION DEFINED minor revision number. */
		uint32_t revision : 4;

		/** @brief IMPLEMENTATION DEFINED variant number, distinguishing
		 * product variants or major revisions. */
		uint32_t variant : 4;

		/** @brief Reserved. */
		uint32_t res0 : 4;

		/** @brief IMPLEMENTATION DEFINED product identifier. */
		uint32_t product_id : 8;
	};
} gicd_iidr_t;
_Static_assert(sizeof(gicd_iidr_t) == 4, "GICD_IIDR must be 32 bits");

/**
 * @brief Interrupt Priority Register entry, GICD_IPRIORITYR<n>.
 *
 * Each register holds the priority of four consecutive INTIDs: for register
 * index n, priority0..3 correspond to INTIDs 4n..4n+3. Lower values are
 * higher priority. The number of bits actually implemented is
 * IMPLEMENTATION DEFINED; unimplemented low-order bits are RAZ/WI.
 */
typedef union gicd_ipriorityr_t {
	/** @brief Raw 32-bit register value. */
	uint32_t raw;
	struct {
		/** @brief Priority of INTID 4n. */
		uint32_t priority0 : 8;

		/** @brief Priority of INTID 4n+1. */
		uint32_t priority1 : 8;

		/** @brief Priority of INTID 4n+2. */
		uint32_t priority2 : 8;

		/** @brief Priority of INTID 4n+3. */
		uint32_t priority3 : 8;
	};
} gicd_ipriorityr_t;

_Static_assert(sizeof(gicd_ipriorityr_t) == 4,
	       "GICD_IPRIORITYR<n> must be 32 bits");

/**
 * @brief Interrupt Routing Mode (IRM) for GICD_IROUTER<n>[31].
 *
 * Controls how Shared Peripheral Interrupts (SPIs) are routed in a
 * multi-core affinity hierarchy.
 *
 * The IRM bit determines whether routing is performed using an explicit
 * affinity path (Aff3.Aff2.Aff1.Aff0) or dynamically to any participating PE.
 */
typedef enum gicd_irm_t {

	/**
	 * @brief Route interrupt using affinity path.
	 *
	 * IRM = 0
	 *
	 * The interrupt is delivered to the Processing Element (PE) whose
	 * affinity values match:
	 * Aff3.Aff2.Aff1.Aff0.
	 *
	 * This provides deterministic routing to a specific CPU.
	 */
	GICD_IRM_AFFINITY = 0b0,

	/**
	 * @brief Route interrupt to any participating PE.
	 *
	 * IRM = 1
	 *
	 * The interrupt is routed to any PE that is part of the affinity
	 * group. The exact target is chosen by the interrupt controller
	 * implementation.
	 */
	GICD_IRM_ANY_PE = 0b1,

} gicd_irm_t;

/**
 * @brief Interrupt Routing Register entry, GICD_IROUTER<n>.
 *
 * Selects the target PE for SPI INTID n by affinity, or broadcasts it to
 * all participating PEs if IRM is set. On a single-core system, leaving
 * the entire register zero routes the interrupt to PE 0.0.0.0.
 */
typedef union gicd_irouter_t {
	/** @brief Raw 64-bit register value. */
	uint64_t raw;
	struct __attribute__((packed)) {
		/** @brief Affinity level 0 of the target PE. */
		uint64_t aff0 : 8;

		/** @brief Affinity level 1 of the target PE. */
		uint64_t aff1 : 8;

		/** @brief Affinity level 2 of the target PE. */
		uint64_t aff2 : 8;

		/** @brief Reserved. */
		uint64_t res0 : 7;

		/** @brief Interrupt Routing Mode. 0: route to
		 * Aff3.Aff2.Aff1.Aff0. 1: route to any participating PE. */
		gicd_irm_t irm : 1;

		/** @brief Affinity level 3 of the target PE. */
		uint64_t aff3 : 8;

		/** @brief Reserved. */
		uint64_t res1 : 24;
	};
} gicd_irouter_t;
_Static_assert(sizeof(gicd_irouter_t) == 8, "GICD_IROUTER<n> must be 64 bits");

/**
 * @brief GICD_ISENABLER<n> — Distributor Interrupt Set-Enable Registers
 *                     (offset 0x0100 + 4n, RW)
 *
 * Each bit enables forwarding of the corresponding SPI INTID.
 * Register n covers INTIDs [32n+31 : 32n].
 *
 *   GICD_ISENABLER0 → INTIDs  0-31  (SGI/PPI range, RAZ/WI)
 *   GICD_ISENABLER1 → INTIDs 32-63  (SPIs start here)
 *   GICD_ISENABLER2 → INTIDs 64-95
 *   ... and so on.
 *
 * Writing 1 enables the interrupt. Writing 0 has no effect.
 * Use GICD_ICENABLER<n> to disable. Resets to 0 on GIC reset.
 */
typedef union gicd_isenabler_t {
	/** @brief Raw 32-bit register value. */
	uint32_t raw;
	struct __attribute__((packed)) {
		/** @brief Bit n enables INTID (32*reg + n).
		 *  Write 1 to enable, write 0 has no effect. */
		uint32_t enable : 32;
	};
} gicd_isenabler_t;
_Static_assert(sizeof(gicd_isenabler_t) == 4, "GICD_ISENABLER must be 32 bits");

/**
 * @brief Distributor Interrupt Clear-Enable Registers, GICD_ICENABLER<n>
 *        (offset 0x0180 + 4n, RW).
 *
 * Same layout as gicd_isenabler_t. Writing 1 to a bit disables forwarding
 * of the corresponding INTID; writing 0 has no effect.
 */
typedef gicd_isenabler_t gicd_icenabler_t;

/**
 * @brief GICD_IGROUPR<n> — Distributor Interrupt Group Registers
 *                   (offset 0x0080 + 4n, RW)
 *
 * Controls Group 0 vs Group 1 assignment for each SPI.
 * Register n covers INTIDs [32n+31 : 32n].
 *
 * Combined with GICD_IGRPMODR<n>:
 *  IGROUPR=0, IGRPMODR=0 → Group 0
 *  IGROUPR=1, IGRPMODR=0 → Group 1 Non-secure
 *  IGROUPR=0, IGRPMODR=1 → Group 1 Secure
 *  IGROUPR=1, IGRPMODR=1 → Reserved
 *
 * Reset to 0 on GIC reset (all INTIDs default to Group 0).
 */
typedef union gicd_igroupr_t {
	/** @brief Raw 32-bit register value. */
	uint32_t raw;
	struct __attribute__((packed)) {
		/** @brief Bit n sets Group for INTID (32*reg + n).
		 *  0 = Group 0, 1 = Group 1 (see GICD_IGRPMODR for Secure/NS).
		 */
		uint32_t group : 32;
	};
} gicd_igroupr_t;
_Static_assert(sizeof(gicd_igroupr_t) == 4, "GICD_IGROUPR must be 32 bits");

/**
 * @brief GICD_IGRPMODR<n> — Distributor Interrupt Group Modifier Registers
 *                    (offset 0x0D00 + 4n, RW)
 *
 * Modifier bits working in conjunction with GICD_IGROUPR<n> to select
 * between Group 1 Secure and Group 1 Non-secure for each SPI.
 * Register n covers INTIDs [32n+31 : 32n].
 *
 * Reset to 0 on GIC reset.
 */
typedef union gicd_igrpmodr_t {
	/** @brief Raw 32-bit register value. */
	uint32_t raw;
	struct __attribute__((packed)) {
		/** @brief Bit n is the Group modifier for INTID (32*reg + n).
		 *  0 = Group 1 NS (when IGROUPR=1), 1 = Group 1 S (when
		 * IGROUPR=0). */
		uint32_t grpmod : 32;
	};
} gicd_igrpmodr_t;
_Static_assert(sizeof(gicd_igrpmodr_t) == 4, "GICD_IGRPMODR must be 32 bits");

/* ── Re-Distributor ───────────────────────────────────────────────────── */
/**
 * @brief GICR_WAKER — Redistributor Wake Register (RD frame, offset 0x0014, RW)
 *
 * Controls and reflects the sleep/wake state of this Redistributor.
 * The boot sequence requires clearing ProcessorSleep and then polling
 * ChildrenAsleep until it reads 0 before the PE can receive interrupts.
 */
typedef union gicr_waker_t {
	/** @brief Raw 32-bit register value. */
	uint32_t raw;
	struct __attribute__((packed)) {
		/** @brief [0] Implementation-defined sleep bit. RES0 on most
		 * impls. */
		bool impl_defined_0 : 1;

		/** @brief [1] ProcessorSleep (RW): Set by software to signal
		 * the PE is entering a low-power state. Clear this to wake the
		 * Redistributor. Must only be changed while ChildrenAsleep
		 * == 1. */
		bool processor_sleep : 1;

		/** @brief [2] ChildrenAsleep (RO): Set by hardware when all CPU
		 *  interface child nodes have entered a quiescent state.
		 *  Poll until this reads 0 after clearing ProcessorSleep. */
		bool children_asleep : 1;

		/** @brief [30:3] Reserved. */
		uint32_t res0 : 28;

		/** @brief [31] Implementation-defined sleep bit. RES0 on most
		 * impls. */
		bool impl_defined_31 : 1;
	};
} gicr_waker_t;
_Static_assert(sizeof(gicr_waker_t) == 4, "GICR_WAKER must be 32 bits");

/**
 * @brief GICR_ISENABLER0 — Redistributor Interrupt Set-Enable Register 0
 *                   (SGI frame, offset 0x0100, RW)
 *
 * Each bit enables forwarding of the corresponding SGI or PPI INTID
 * to the CPU interface for this PE.
 *
 * Writing 1 enables forwarding. Writing 0 has no effect.
 * Use GICR_ICENABLER0 to disable individual interrupts.
 * Resets to 0 on GIC reset — all SGIs and PPIs are disabled by default
 * and must be explicitly enabled by software.
 *
 * Bit layout:
 *  [15:0]  SGIs (INTIDs  0-15) : RW — reset to 0, write 1 to enable
 *  [31:16] PPIs (INTIDs 16-31) : RW — reset to 0, write 1 to enable
 */
typedef union gicr_isenabler0_t {
	/** @brief Raw 32-bit register value. */
	uint32_t raw;
	struct __attribute__((packed)) {
		/** @brief [15:0] SGI enable bits (INTIDs 0-15).
		 *  Reset to 0. Write 1 to enable forwarding of the
		 * corresponding SGI to the CPU interface. Write 0 has no
		 * effect. */
		uint32_t sgi_enable : 16;

		/** @brief [31:16] PPI enable bits (INTIDs 16-31).
		 *  Reset to 0. Write 1 to enable forwarding of the
		 * corresponding PPI to the CPU interface. Write 0 has no
		 * effect. */
		uint32_t ppi_enable : 16;
	};
} gicr_isenabler0_t;
_Static_assert(sizeof(gicr_isenabler0_t) == 4,
	       "GICR_ISENABLER0 must be 32 bits");

/**
 * @brief GICR_IGROUPR0 — Redistributor Interrupt Group Register 0
 *                 (SGI frame, offset 0x0080, RW)
 *
 * Controls the Group assignment of each SGI and PPI for this PE.
 * Each bit selects whether the corresponding INTID belongs to
 * Group 0 or Group 1.
 *
 * Bit values:
 *  0 = Group 0  (secure, signaled as FIQ)
 *  1 = Group 1  (non-secure or secure depending on GICR_IGRPMODR0)
 *
 * Combined with GICR_IGRPMODR0:
 *  IGROUPR0=0, IGRPMODR0=0 → Group 0
 *  IGROUPR0=1, IGRPMODR0=0 → Group 1 Non-secure
 *  IGROUPR0=0, IGRPMODR0=1 → Group 1 Secure
 *  IGROUPR0=1, IGRPMODR0=1 → Reserved
 *
 * Reset to 0 on GIC reset (all SGIs/PPIs default to Group 0).
 *
 */
typedef union gicr_igroupr0_t {
	/** @brief Raw 32-bit register value. */
	uint32_t raw;
	struct __attribute__((packed)) {
		/** @brief [15:0] Group bits for SGIs (INTIDs 0-15).
		 *  0 = Group 0, 1 = Group 1 (see GICR_IGRPMODR0 for Secure/NS).
		 */
		uint32_t sgi_group : 16;

		/** @brief [31:16] Group bits for PPIs (INTIDs 16-31).
		 *  0 = Group 0, 1 = Group 1 (see GICR_IGRPMODR0 for Secure/NS).
		 */
		uint32_t ppi_group : 16;
	};
} gicr_igroupr0_t;
_Static_assert(sizeof(gicr_igroupr0_t) == 4, "GICR_IGROUPR0 must be 32 bits");

/**
 * @brief GICR_IGRPMODR0 — Redistributor Interrupt Group Modifier Register 0
 *                  (SGI frame, offset 0x0D00, RW)
 *
 * Modifier bits that work in conjunction with GICR_IGROUPR0 to select
 * between Group 1 Secure and Group 1 Non-secure for each SGI and PPI.
 *
 * Combined with GICR_IGROUPR0:
 *  IGROUPR0=0, IGRPMODR0=0 → Group 0
 *  IGROUPR0=1, IGRPMODR0=0 → Group 1 Non-secure
 *  IGROUPR0=0, IGRPMODR0=1 → Group 1 Secure
 *  IGROUPR0=1, IGRPMODR0=1 → Reserved
 *
 * Reset to 0 on GIC reset.
 *
 */
typedef union gicr_igrpmodr0_t {
	/** @brief Raw 32-bit register value. */
	uint32_t raw;
	struct __attribute__((packed)) {
		/** @brief [15:0] Group modifier bits for SGIs (INTIDs 0-15). */
		uint32_t sgi_grpmod : 16;

		/** @brief [31:16] Group modifier bits for PPIs (INTIDs 16-31).
		 */
		uint32_t ppi_grpmod : 16;
	};
} gicr_igrpmodr0_t;
_Static_assert(sizeof(gicr_igrpmodr0_t) == 4, "GICR_IGRPMODR0 must be 32 bits");

/**
 * @brief GICR_IPRIORITYR — Redistributor Interrupt Priority Registers
 *                   (SGI frame, offset 0x0400-0x041C, RW)
 *
 * Eight 32-bit registers, each holding the 8-bit priority of 4 INTIDs.
 * Covers all 32 SGIs and PPIs (INTIDs 0-31).
 *
 * Register n covers INTIDs [4n+3 : 4n]:
 *   GICR_IPRIORITYR0 → INTIDs  0- 3  (offset 0x0400)
 *   GICR_IPRIORITYR1 → INTIDs  4- 7  (offset 0x0404)
 *   GICR_IPRIORITYR2 → INTIDs  8-11  (offset 0x0408)
 *   GICR_IPRIORITYR3 → INTIDs 12-15  (offset 0x040C)
 *   GICR_IPRIORITYR4 → INTIDs 16-19  (offset 0x0410)
 *   GICR_IPRIORITYR5 → INTIDs 20-23  (offset 0x0414)
 *   GICR_IPRIORITYR6 → INTIDs 24-27  (offset 0x0418)
 *   GICR_IPRIORITYR7 → INTIDs 28-31  (offset 0x041C)
 *
 * Lower value = higher priority. Only the high bits are implemented
 * (determined by ICC_CTLR_EL1.PRIbits); unimplemented low bits are RAZ/WI.
 *
 */
typedef union gicr_ipriorityr_t {
	/** @brief Raw 32-bit register value. */
	uint32_t raw;
	struct __attribute__((packed)) {
		/** @brief [7:0]   Priority of INTID (4n+0). */
		uint32_t prio_0 : 8;

		/** @brief [15:8]  Priority of INTID (4n+1). */
		uint32_t prio_1 : 8;

		/** @brief [23:16] Priority of INTID (4n+2). */
		uint32_t prio_2 : 8;

		/** @brief [31:24] Priority of INTID (4n+3). */
		uint32_t prio_3 : 8;
	};
} gicr_ipriorityr_t;
_Static_assert(sizeof(gicr_ipriorityr_t) == 4,
	       "GICR_IPRIORITYR must be 32 bits");

/**
 * @brief ICC_SRE_EL1 — Interrupt Controller System Register Enable (EL1).
 *
 * Controls whether the CPU interface registers are accessed via the
 * memory-mapped GICC interface or the AArch64 ICC_* system registers.
 * SRE must be set to 1 before any other ICC_* register can be used.
 *
 * Note: If EL3 is present, ICC_SRE_EL3.SRE must also be set first,
 * and ICC_SRE_EL2.SRE/Enable must be set to allow EL1 access.
 */
typedef union icc_sre_el1_t {
	/** @brief Raw 64-bit register value. */
	uint64_t raw;
	struct __attribute__((packed)) {
		/** @brief [0] SRE: System Register Enable. Must be 1 to use
		 * ICC_* system registers. When 0, GICC memory-mapped registers
		 * are used instead. RAO/WI if EL2/EL3 have already forced it
		 * enabled. */
		bool sre : 1;

		/** @brief [1] DFB: Disable FIQ Bypass. When 1, FIQ signals are
		 * not bypassed to the CPU. Should be set to 1 in most OS
		 * kernels. */
		bool dfb : 1;

		/** @brief [2] DIB: Disable IRQ Bypass. When 1, IRQ signals are
		 * not bypassed to the CPU. Should be set to 1 in most OS
		 * kernels. */
		bool dib : 1;

		/** @brief [63:3] Reserved. */
		uint64_t res0 : 61;
	};
} icc_sre_el1_t;
_Static_assert(sizeof(icc_sre_el1_t) == 8, "ICC_SRE_EL1 must be 64 bits");

/**
 * @brief ICC_CTLR_EL1 — Interrupt Controller Control Register (EL1).
 *
 * Controls the behavior of the CPU interface for the current Security
 * state. Key fields are EOImode (split vs priority-drop-and-deactivate)
 * and read-only capability fields populated by hardware.
 *
 * Verified against ARM IHI0069 (GICv3 Architecture Specification).
 */
typedef union icc_ctlr_el1_t {
	/** @brief Raw 64-bit register value. */
	uint64_t raw;
	struct __attribute__((packed)) {
		/** @brief [0] CBPR: Common Binary Point Register.
		 *  0 = ICC_BPR0_EL1 and ICC_BPR1_EL1 are independent.
		 *  1 = ICC_BPR0_EL1 is used for both Group 0 and Group 1. */
		bool cbpr : 1;

		/** @brief [1] EOImode: End of Interrupt mode.
		 *  0 = ICC_EOIR1_EL1 performs priority drop AND deactivation.
		 *  1 = ICC_EOIR1_EL1 performs priority drop only; deactivation
		 *      requires a separate write to ICC_DIR_EL1. */
		bool eoi_mode : 1;

		/** @brief [5:2] Reserved. */
		uint32_t res0_5_2 : 4;

		/** @brief [6] PMHE: Priority Mask Hint Enable.
		 *  When 1, the priority mask register (ICC_PMR_EL1) is used as
		 *  a hint for interrupt routing in power management scenarios.
		 *  RAZ/WI if the implementation does not support this feature.
		 */
		bool pmhe : 1;

		/** @brief [7] Reserved. */
		uint32_t res0_7 : 1;

		/** @brief [10:8] PRIbits (RO): Number of priority bits
		 * implemented, minus one. A value of N means N+1 priority bits
		 * are implemented, giving 2^(N+1) priority levels. E.g. value
		 * of 4 means 4+1 = 5 bits = 32 priority levels. */
		uint32_t pri_bits : 3;

		/** @brief [13:11] IDbits (RO): Number of interrupt identifier
		 * bits supported. 0b000 = 16-bit, 0b001 = 24-bit. */
		uint32_t id_bits : 3;

		/** @brief [14] SEIS (RO): Supports generation of SEIs (System
		 * Error Interrupts) by the CPU interface. */
		bool seis : 1;

		/** @brief [15] A3V (RO): Affinity 3 Valid. When 1, non-zero
		 * Aff3 values are supported in SGI routing. */
		bool a3v : 1;

		/** @brief [17:16] Reserved. */
		uint32_t res0_17_16 : 2;

		/** @brief [18] RSS: Range Selector Support (RO).
		 *  When 1, ICC_SGI0R_EL1 and ICC_SGI1R_EL1 support the RS field
		 *  for targeting SGIs to PEs with Aff3 values 0-15 and 16-31.
		 */
		bool rss : 1;

		/** @brief [19] ExtRange: Extended INTID range supported (RO).
		 *  When 1, extended SPI/PPI ranges (INTIDs 1024-8191) are
		 *  supported by this CPU interface. */
		bool ext_range : 1;

		/** @brief [63:20] Reserved. */
		uint64_t res0_63_20 : 44;
	};
} icc_ctlr_el1_t;
_Static_assert(sizeof(icc_ctlr_el1_t) == 8, "ICC_CTLR_EL1 must be 64 bits");

/**
 * @brief ICC_PMR_EL1 — Interrupt Controller Priority Mask Register (EL1).
 *
 * Sets the minimum priority threshold for interrupt delivery to the PE.
 * Only interrupts with a priority numerically LESS THAN this value are
 * forwarded to the PE. Set to 0xFF to allow all priorities through.
 *
 * Note: In a two-security-state system, Non-secure reads/writes to this
 * register may see a shifted view of the priority (bit 7 forced to 1).
 */
typedef union icc_pmr_el1_t {
	/** @brief Raw 64-bit register value. */
	uint64_t raw;
	struct __attribute__((packed)) {
		/** @brief [7:0] Priority: The priority mask level. Interrupts
		 * with a priority value >= this threshold are suppressed. 0x00
		 * = No interrupts allowed through. 0xFF = All interrupts
		 * allowed through. */
		uint32_t priority : 8;

		/** @brief [63:8] Reserved. */
		uint64_t res0 : 56;
	};
} icc_pmr_el1_t;
_Static_assert(sizeof(icc_pmr_el1_t) == 8, "ICC_PMR_EL1 must be 64 bits");

/**
 * @brief ICC_BPR1_EL1 — Interrupt Controller Binary Point Register 1 (EL1).
 *
 * Defines the split between the priority group field and the subpriority
 * field for Group 1 interrupts. Controls preemption behavior.
 *
 * The binary point value N means:
 *   - Priority group  = priority[7:N+1]  (determines preemption)
 *   - Subpriority     = priority[N:0]    (no preemption effect)
 *
 * Setting BinaryPoint=0 gives the finest preemption granularity (all
 * priority bits participate in preemption). Higher values reduce
 * preemption sensitivity.
 */
typedef union icc_bpr1_el1_t {
	/** @brief Raw 64-bit register value. */
	uint64_t raw;
	struct __attribute__((packed)) {
		/** @brief [2:0] BinaryPoint: The binary point split value
		 * (0–7). Minimum value is implementation-defined (read PRIbits
		 * from ICC_CTLR_EL1 to determine the floor). */
		uint32_t binary_point : 3;

		/** @brief [63:3] Reserved. */
		uint64_t res0 : 61;
	};
} icc_bpr1_el1_t;
_Static_assert(sizeof(icc_bpr1_el1_t) == 8, "ICC_BPR1_EL1 must be 64 bits");

/**
 * @brief ICC_IGRPEN1_EL1 — Interrupt Controller Group 1 Enable (EL1).
 *
 * Enables or disables forwarding of Group 1 interrupts to the PE for
 * the current Security state. This is the final gate before interrupts
 * are actually signaled to the processor.
 *
 * Note: The Distributor (GICD_CTLR.EnableGrp1NS) and Redistributor
 * (GICR_CTLR DPG bits) must also permit the interrupt for delivery to
 * occur. All three gates must be open.
 */
typedef union icc_igrpen1_el1_t {
	/** @brief Raw 64-bit register value. */
	uint64_t raw;
	struct __attribute__((packed)) {
		/** @brief [0] Enable: Group 1 interrupt forwarding enable.
		 *  0 = Group 1 interrupts are not forwarded to this PE.
		 *  1 = Group 1 interrupts are forwarded to this PE. */
		bool enable : 1;

		/** @brief [63:1] Reserved. */
		uint64_t res0 : 63;
	};
} icc_igrpen1_el1_t;
_Static_assert(sizeof(icc_igrpen1_el1_t) == 8,
	       "ICC_IGRPEN1_EL1 must be 64 bits");

/**
 * @brief ICC_IAR1_EL1 — Interrupt Controller Interrupt Acknowledge Register 1.
 *
 * Read-only. Reading this register acknowledges the highest-priority
 * pending Group 1 interrupt and returns its INTID. This simultaneously
 * moves the interrupt from pending to active state.
 *
 * Special INTIDs:
 *  - 1020 : Interrupt was a Group 0 interrupt (spurious for Group 1 read)
 *  - 1021 : Interrupt was a Non-secure Group 1 (from Secure state read)
 *  - 1022 : Reserved
 *  - 1023 : Spurious interrupt (no pending interrupt, or PMR/enable masked)
 */
typedef union icc_iar1_el1_t {
	/** @brief Raw 64-bit register value. */
	uint64_t raw;
	struct __attribute__((packed)) {
		/** @brief [23:0] INTID: The INTID of the acknowledged
		 * interrupt. Values 1020–1023 are special spurious/group
		 * indicators. Always check for 1023 before processing. */
		uint32_t intid : 24;

		/** @brief [63:24] Reserved. */
		uint64_t res0 : 40;
	};
} icc_iar1_el1_t;
_Static_assert(sizeof(icc_iar1_el1_t) == 8, "ICC_IAR1_EL1 must be 64 bits");

/**
 * @brief ICC_EOIR1_EL1 — Interrupt Controller End Of Interrupt Register 1.
 *
 * Write-only. Writing the INTID obtained from ICC_IAR1_EL1 signals
 * End of Interrupt for the corresponding Group 1 interrupt.
 *
 * When ICC_CTLR_EL1.EOImode == 0: performs priority drop AND deactivation.
 * When ICC_CTLR_EL1.EOImode == 1: performs priority drop ONLY; a separate
 * write to ICC_DIR_EL1 is required for deactivation.
 *
 * Note: Always write the INTID value read from ICC_IAR1_EL1, not a
 * modified value. Writing an incorrect INTID is UNPREDICTABLE.
 */
typedef union icc_eoir1_el1_t {
	/** @brief Raw 64-bit register value. */
	uint64_t raw;
	struct __attribute__((packed)) {
		/** @brief [23:0] INTID: The INTID to deactivate/drop priority
		 * for. Must match the value previously read from ICC_IAR1_EL1.
		 */
		uint32_t intid : 24;

		/** @brief [63:24] Reserved. */
		uint64_t res0 : 40;
	};
} icc_eoir1_el1_t;
_Static_assert(sizeof(icc_eoir1_el1_t) == 8, "ICC_EOIR1_EL1 must be 64 bits");

/** @brief Memory map for GICv3 register regions. */
typedef struct gicv3_memap_t {
	/** @brief Base address of the distributor region. */
	volatile virt_addr dist_base;

	/** @brief Base address of the redistributor region. */
	volatile virt_addr redist_base;
} gicv3_memap_t;

/**
 * @brief Initialize the ARM GICv3 interrupt controller.
 *
 * @param fdt  Pointer to the Flattened Device Tree blob used to discover the
 *             interrupt controller configuration.
 */
void gicv3_init(void *fdt);

/**
 * @brief Register a handler for a hardware interrupt source and enable it.
 *
 * Installs the given handler and private data into the dispatch table for
 * the specified INTID, then configures and enables the interrupt in the
 * GIC. The handler will be invoked with private_data on every subsequent
 * assertion of this interrupt.
 *
 * @param irq_num      Physical interrupt INTID to register (0-1019).
 * @param handler_data Handler function and private data to register.
 * @return true if the handler was registered and the interrupt enabled
 *         successfully, false if the INTID is out of range or GIC
 *         configuration failed.
 */
bool gicv3_register_irq(uint32_t irq_num, handler_data_t handler_data);

/**
 * @brief Unregister a handler for a hardware interrupt source and disable it.
 *
 * Disables the interrupt in the GIC and clears the corresponding entry
 * in the dispatch table. After this call the INTID will no longer fire
 * and any previously registered handler will not be invoked.
 *
 * @param irq_num Physical interrupt INTID to unregister (0-1019).
 */
void gicv3_unregister_irq(uint32_t irq_num);

/**
 * @brief EL1 IRQ handler.
 *
 * Acknowledges the interrupt by reading ICC_IAR1_EL1, dispatches based
 * on INTID, then signals EOI via ICC_EOIR1_EL1.
 *
 * Must be called with IRQs already masked (hardware does this automatically
 * on exception entry). IRQs are re-enabled on exception return (ERET).
 */
void gicv3_handle_irq(void);

#endif /* DRIVERS_GICV3_H */
