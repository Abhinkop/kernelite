/**
 * @file gicv3.c
 * @brief ARM GICv3 interrupt controller support.
 *
 * Provides an abstraction layer for initializing and managing the ARM GICv3
 * interrupt controller. This file is intentionally minimal and may be extended
 * with platform-specific hardware access routines.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-06-14
 */

#include "gicv3.h"

#include "utils/kprintf.h"
#include "utils/utils.h"
#include "fdt/fdt.h"
#include "utils/string.h"
#include "page_table/page_table.h"
#include "linker/symbols.h"
#include "mem_layout/mem_layout.h"
#include "asm/asm_helper.h"
#include "icu/icu.h"

#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

/** @brief Maximum number of interrupt INTIDs supported by the dispatch table.
 *  Covers SGIs (0-15), PPIs (16-31), and SPIs (32-1019). INTIDs 1020-1023
 *  are reserved for spurious interrupt signalling and are never dispatched. */
#define MAX_INT_ID 1024

/**
 * @brief Global interrupt dispatch table indexed by INTID.
 *
 * Each entry holds the handler and private data for the corresponding
 * INTID. Entries are zeroed on startup (handler == NULL means unregistered).
 */
static handler_data_t dispatch_table[MAX_INT_ID];

/**
 * @brief Represents the memory-mapped base address of the GICv3 interrupt
 * controller.
 */
static volatile struct gicv3_memap_t memap;

/**
 * @brief Get GICD_CTLR register.
 * @return Volatile pointer to GICD_CTLR.
 */
static inline volatile gicd_ctlr_t *get_gicd_ctlr(void)
{
	// NOLINTNEXTLINE(*int-to-ptr)
	return (volatile gicd_ctlr_t *)(memap.dist_base + 0x0000);
}

/**
 * @brief Get GICD_TYPER register.
 * @return Volatile pointer to GICD_TYPER.
 */
static inline volatile gicd_typer_t *get_gicd_typer(void)
{
	// NOLINTNEXTLINE(*int-to-ptr)
	return (volatile gicd_typer_t *)(memap.dist_base + 0x0004);
}

/**
 * @brief Get GICD_IIDR register.
 * @return Volatile pointer to GICD_IIDR.
 */
static inline volatile gicd_iidr_t *get_gicd_iidr(void)
{
	// NOLINTNEXTLINE(*int-to-ptr)
	return (volatile gicd_iidr_t *)(memap.dist_base + 0x0008);
}

/**
 * @brief Get GICD_IGROUPR<n> (offset 0x0080 + 4n, RW).
 * Group assignment register for INTIDs [32n+31:32n].
 * @param n Register index.
 * @return Volatile pointer to GICD_IGROUPR<n>.
 */
static inline volatile gicd_igroupr_t *get_gicd_igroupr(const uint32_t n)
{
	// NOLINTNEXTLINE(*int-to-ptr)
	return (volatile gicd_igroupr_t *)(memap.dist_base + 0x0080 +
					   (virt_addr)(n * 4));
}

/**
 * @brief Get GICD_IGRPMODR<n> (offset 0x0D00 + 4n, RW).
 * Group modifier register for INTIDs [32n+31:32n].
 * @param n Register index.
 * @return Volatile pointer to GICD_IGRPMODR<n>.
 */
static inline volatile gicd_igrpmodr_t *get_gicd_igrpmodr(const uint32_t n)
{
	// NOLINTNEXTLINE(*int-to-ptr)
	return (volatile gicd_igrpmodr_t *)(memap.dist_base + 0x0D00 +
					    (virt_addr)(n * 4));
}

/**
 * @brief Get GICD_IPRIORITYR<n> (offset 0x0400 + 4n, RW).
 * Priority register covering INTIDs [4n+3:4n].
 * @param n Register index.
 * @return Volatile pointer to GICD_IPRIORITYR<n>.
 */
static inline volatile gicd_ipriorityr_t *get_gicd_ipriorityr(const uint32_t n)
{
	// NOLINTNEXTLINE(*int-to-ptr)
	return (volatile gicd_ipriorityr_t *)(memap.dist_base + 0x0400 +
					      (virt_addr)(n * 4));
}

/**
 * @brief Get GICD_IROUTER<n> (offset 0x6000 + 8n, RW).
 * Affinity routing register for SPI INTID n (n >= 32).
 * @param intid SPI INTID (must be >= 32).
 * @return Volatile pointer to GICD_IROUTER<n>.
 */
static inline volatile gicd_irouter_t *get_gicd_irouter(const uint32_t intid)
{
	// NOLINTNEXTLINE(*int-to-ptr)
	return (volatile gicd_irouter_t *)(memap.dist_base + 0x6000 +
					   (virt_addr)(intid * 8));
}

/**
 * @brief Get GICD_ISENABLER<n> (offset 0x0100 + 4n, RW).
 * Set-Enable register for INTIDs [32n+31:32n].
 * @param n Register index (0 = INTIDs 0-31, 1 = INTIDs 32-63, ...).
 * @return Volatile pointer to GICD_ISENABLER<n>.
 */
static inline volatile gicd_isenabler_t *get_gicd_isenabler(const uint32_t n)
{
	// NOLINTNEXTLINE(*int-to-ptr)
	return (volatile gicd_isenabler_t *)(memap.dist_base + 0x0100 +
					     (virt_addr)(n * 4));
}

/**
 * @brief Get GICD_ICENABLER<n> (offset 0x0180 + 4n, RW).
 * Clear-Enable register for INTIDs [32n+31:32n].
 * @param n Register index (0 = INTIDs 0-31, 1 = INTIDs 32-63, ...).
 * @return Volatile pointer to GICD_ICENABLER<n>.
 */
static inline volatile gicd_icenabler_t *get_gicd_icenabler(const uint32_t n)
{
	// NOLINTNEXTLINE(*int-to-ptr)
	return (volatile gicd_icenabler_t *)(memap.dist_base + 0x0180 +
					     (virt_addr)(n * 4));
}

/**
 * @brief Get Re-disturbutor base.
 * @param cpu_num the core number.
 * @return the virtual address of the base
 */
static virt_addr get_redist_base(const uint32_t cpu_num)
{
	const uint64_t redist_stride = 0x20000UL;
	return memap.redist_base + (cpu_num * redist_stride);
}

/**
 * @brief Get the SGI frame base address for the given Redistributor.
 *
 * Each Redistributor has two 64KB frames:
 *   - RD frame  (offset 0x00000): GICR_CTLR, GICR_TYPER, GICR_WAKER, etc.
 *   - SGI frame (offset 0x10000): GICR_IGROUPR0, GICR_ISENABLER0, etc.
 *
 * This function returns the base of the SGI frame for the given PE,
 * which is always 64KB above the RD frame base.
 *
 * @param cpu_num The redistributor index (0-based PE number).
 * @return Virtual address of the SGI frame base for this PE.
 */
static virt_addr get_gicr_sgi_base(const uint32_t cpu_num)
{
	return get_redist_base(cpu_num) + 0x10000;
}

/**
 * @brief Get GICR_WAKER register (RD frame, offset 0x0014).
 * Controls Redistributor wake state. Clear ProcessorSleep then poll
 * ChildrenAsleep == 0 before enabling interrupts.
 * @param cpu_num The redistributor index (0-based PE number).
 * @return Volatile pointer to GICR_WAKER for this PE.
 */
static inline volatile gicr_waker_t *get_gicr_waker(const uint32_t cpu_num)
{
	// NOLINTNEXTLINE(*int-to-ptr)
	return (volatile gicr_waker_t *)(get_redist_base(cpu_num) + 0x0014);
}

/**
 * @brief Get GICR_ISENABLER0 register (SGI frame, offset 0x0100, RW).
 *
 * Set-Enable register controlling SGI and PPI interrupt forwarding for
 * this PE. Writing 1 to a bit enables the corresponding INTID; writing
 * 0 has no effect. Use GICR_ICENABLER0 to disable individual interrupts.
 *
 * Bit layout:
 *  [15:0]  SGIs (INTIDs  0-15) : RAO/WI — permanently enabled
 *  [31:16] PPIs (INTIDs 16-31) : RW     — write 1 to enable
 *
 * @param cpu_num The redistributor index (0-based PE number).
 * @return Volatile pointer to GICR_ISENABLER0 for this PE.
 */
static inline volatile gicr_isenabler0_t *
get_gicr_isenabler0(const uint32_t cpu_num)
{
	// NOLINTNEXTLINE(*int-to-ptr)
	return (volatile gicr_isenabler0_t *)(get_gicr_sgi_base(cpu_num) +
					      0x100);
}

/**
 * @brief Get GICR_IGROUPR0 register (SGI frame, offset 0x0080, RW).
 *
 * Controls Group 0 vs Group 1 assignment for all SGIs and PPIs.
 * Must be used in conjunction with GICR_IGRPMODR0 to select between
 * Group 1 Secure and Group 1 Non-secure.
 *
 * @param cpu_num The redistributor index (0-based PE number).
 * @return Volatile pointer to GICR_IGROUPR0 for this PE.
 */
static inline volatile gicr_igroupr0_t *
get_gicr_igroupr0(const uint32_t cpu_num)
{
	// NOLINTNEXTLINE(*int-to-ptr)
	return (volatile gicr_igroupr0_t *)(get_gicr_sgi_base(cpu_num) +
					    0x0080);
}

/**
 * @brief Get GICR_IGRPMODR0 register (SGI frame, offset 0x0D00, RW).
 *
 * Modifier bits selecting between Group 1 Secure and Group 1 Non-secure
 * for each SGI and PPI. Works in conjunction with GICR_IGROUPR0.
 *
 * @param cpu_num The redistributor index (0-based PE number).
 * @return Volatile pointer to GICR_IGRPMODR0 for this PE.
 */
static inline volatile gicr_igrpmodr0_t *
get_gicr_igrpmodr0(const uint32_t cpu_num)
{
	// NOLINTNEXTLINE(*int-to-ptr)
	return (volatile gicr_igrpmodr0_t *)(get_gicr_sgi_base(cpu_num) +
					     0x0D00);
}

/**
 * @brief Get a GICR_IPRIORITYR register (SGI frame, offset 0x0400+n*4, RW).
 *
 * Returns a pointer to the priority register covering INTIDs [4n+3:4n].
 * Valid indices are 0-7, covering all 32 SGIs and PPIs.
 *
 * @param cpu_num The redistributor index (0-based PE number).
 * @param n       Register index (0-7).
 * @return Volatile pointer to GICR_IPRIORITYR[n] for this PE.
 */
static inline volatile gicr_ipriorityr_t *
get_gicr_ipriorityr(const uint32_t cpu_num, const uint32_t n)
{
	// NOLINTNEXTLINE(*int-to-ptr)
	return (volatile gicr_ipriorityr_t *)(get_gicr_sgi_base(cpu_num) +
					      0x0400 + (virt_addr)(n * 4));
}

/**
 * @brief Print decoded GICD_TYPER register for debugging.
 *
 * This function is intended purely for bring-up/debugging and prints
 * human-readable interpretation of the GICD_TYPER register fields.
 */
static inline void print_gicd_typer(void)
{
	volatile gicd_typer_t *typer = get_gicd_typer();

	kprintf("-------------- GIC TYPER -----------------\n");

	kprintf("ITLinesNumber : %u (max SPI interrupts = %u)\n",
		typer->itlinesnumber, (typer->itlinesnumber + 1) * 32);

	kprintf("CPUNumber     : %u (number of CPUs / affinity level 0)\n",
		typer->cpunumber);

	kprintf("ESPI          : %s\n",
		typer->espi ? "Extended SPI range is supported" :
			      "Extended SPI range is NOT supported");

	kprintf("NMI           : %s\n",
		typer->nmi ?
			"Non-maskable interrupt support is implemented" :
			"Non-maskable interrupt support is NOT implemented");

	kprintf("SecurityExtn  : %s\n",
		typer->security_extn ?
			"Security extensions (Group0/Group1 split) are implemented" :
			"Security extensions are NOT implemented (secure-disabled model)");

	kprintf("num_LPIs      : %u (maximum number of LPI interrupts supported)\n",
		typer->num_lpis);

	kprintf("MBIS          : %s\n",
		typer->mbis ?
			"Message-based interrupt generation via Distributor registers is supported" :
			"Message-based interrupt generation via Distributor registers is NOT supported");

	kprintf("LPIs          : %s\n",
		typer->lpis ?
			"Locality-specific interrupts (LPIs) are supported" :
			"LPIs are NOT supported");

	kprintf("DVIS          : %s\n",
		typer->dvis ? "Direct Virtual LPI injection is supported" :
			      "Direct Virtual LPI injection is NOT supported");

	kprintf("IDbits        : %u (implemented INTID bits)\n", typer->idbits);

	kprintf("A3V           : %s\n",
		typer->a3v ?
			"Affinity level 3 is implemented" :
			"Affinity level 3 is fixed to 0 (not implemented)");

	kprintf("No1N          : %s\n",
		typer->no1n ? "1-of-N SPI interrupts are NOT supported" :
			      "1-of-N SPI interrupts are supported");

	kprintf("RSS           : %s\n",
		typer->rss ?
			"Targeted SGIs support extended affinity routing (0-255)" :
			"Targeted SGIs support legacy SGI affinity routing (0-15)");

	kprintf("ESPI_range    : %u (extended SPI range size)\n",
		typer->espi_range);

	kprintf("Raw           : 0x%x\n", (uint32_t)typer->raw);

	kprintf("-----------------------------------------\n\n");
}

/**
 * @brief Dump and decode the GICD_IIDR register for debugging.
 *
 * The GICD_IIDR register identifies the GIC implementation vendor and
 * versioning information. This function decodes and prints it in a
 * human-readable format for bring-up and diagnostics.
 *
 * Fields:
 *  - Implementer : JEP106 vendor ID
 *  - Revision    : Implementation revision
 *  - Variant     : Major implementation variant
 *  - Product ID  : Vendor-defined product identifier
 */
static inline void print_gicd_iidr(void)
{
	volatile gicd_iidr_t *ptr = get_gicd_iidr();

	kprintf("-------------- GIC IIDR -----------------\n");

	/* Vendor decoding */
	switch (ptr->implementer) {
	case 0x41:
		kprintf("Implementer : 0x%lx (ARM Ltd.)\n",
			(unsigned long)ptr->implementer);
		break;

	case 0x43B:
		kprintf("Implementer : 0x%lx (ARM / Cortex-style GIC)\n",
			(unsigned long)ptr->implementer);
		break;

	default:
		kprintf("Implementer : 0x%lx (Unknown JEP106 vendor)\n",
			(unsigned long)ptr->implementer);
		break;
	}

	kprintf("Revision    : %u (implementation revision)\n", ptr->revision);

	kprintf("Variant     : %u (major implementation variant)\n",
		ptr->variant);

	kprintf("Product ID  : 0x%x (implementation-specific ID)\n",
		ptr->product_id);

	kprintf("Raw         : 0x%x\n", (uint32_t)ptr->raw);

	kprintf("-----------------------------------------\n\n");
}

/**
 * @brief Dump and decode the GICD_CTLR register for debugging.
 *
 * The GICD_CTLR register controls the global enable state of the GIC
 * distributor and the routing of Group 0, Secure Group 1, and Non-secure
 * Group 1 interrupts. This function decodes and prints it in a human-readable
 * format for bring-up and diagnostics.
 *
 * Fields (GICv3, with two security states):
 *  - EnableGrp0   : Enable Group 0 interrupts
 *  - EnableGrp1NS : Enable Non-secure Group 1 interrupts
 *  - EnableGrp1S  : Enable Secure Group 1 interrupts (RES0 if ARE_S == 0)
 *  - ARE_S        : Affinity Routing Enable (Secure state)
 *  - ARE_NS       : Affinity Routing Enable (Non-secure state)
 *  - DS           : Disable Security (RAO/WI in single security state)
 *  - E1NWF        : Enable 1-of-N Wakeup Functionality
 *  - RWP          : Register Write Pending (read-only status bit)
 */
static inline void print_gicd_ctlr(void)
{
	gicd_ctlr_t gicd_ctlr;
	gicd_ctlr.raw = get_gicd_ctlr()->raw;

	kprintf("-------------- GIC CTLR -----------------\n");

	kprintf("EnableGrp0  : %u (%s)\n", gicd_ctlr.enable_grp0,
		gicd_ctlr.enable_grp0 ? "Group 0 interrupts ENABLED" :
					"Group 0 interrupts DISABLED");

	kprintf("EnableGrp1NS: %u (%s)\n", gicd_ctlr.enable_grp1ns,
		gicd_ctlr.enable_grp1ns ?
			"Non-secure Group 1 interrupts ENABLED" :
			"Non-secure Group 1 interrupts DISABLED");

	kprintf("EnableGrp1S : %u (%s)\n", gicd_ctlr.enable_grp1s,
		gicd_ctlr.enable_grp1s ?
			"Secure Group 1 interrupts ENABLED" :
			"Secure Group 1 interrupts DISABLED (or RES0)");

	kprintf("ARE_S       : %u (%s)\n", gicd_ctlr.are_s,
		gicd_ctlr.are_s ? "Affinity Routing ENABLED  (Secure)" :
				  "Affinity Routing DISABLED (Secure)");

	kprintf("ARE_NS      : %u (%s)\n", gicd_ctlr.are_ns,
		gicd_ctlr.are_ns ? "Affinity Routing ENABLED  (Non-secure)" :
				   "Affinity Routing DISABLED (Non-secure)");

	kprintf("DS          : %u (%s)\n", gicd_ctlr.ds,
		gicd_ctlr.ds ?
			"Security DISABLED (single security state / RAO+WI)" :
			"Security ENABLED  (two security states)");

	kprintf("E1NWF       : %u (%s)\n", gicd_ctlr.e1nwf,
		gicd_ctlr.e1nwf ? "1-of-N Wakeup ENABLED" :
				  "1-of-N Wakeup DISABLED");

	kprintf("RWP         : %u (%s)\n", gicd_ctlr.rwp,
		gicd_ctlr.rwp ?
			"Write PENDING  (poll until clear before next write)" :
			"Write COMPLETE (safe to proceed)");

	kprintf("Raw         : 0x%x\n", gicd_ctlr.raw);

	kprintf("-----------------------------------------\n\n");
}

/**
 * @brief Dump and decode the GICR_WAKER register for debugging.
 *
 * The critical register for Redistributor bring-up. During boot you must
 * clear ProcessorSleep and poll until ChildrenAsleep reads 0 before the
 * PE can receive any interrupts.
 *
 * Fields:
 *  - ProcessorSleep  : RW — software signals the PE is sleeping
 *  - ChildrenAsleep  : RO — hardware confirms all children are quiescent
 *
 * @param cpu_num The redistributor index (0-based PE number).
 */
static inline void print_gicr_waker(const uint32_t cpu_num)
{
	gicr_waker_t snap;
	snap.raw = get_gicr_waker(cpu_num)->raw;

	kprintf("-------------- GICR_WAKER (cpu %u) ------\n", cpu_num);

	kprintf("ProcessorSleep : %u (%s)\n", snap.processor_sleep,
		snap.processor_sleep ?
			"PE marked ASLEEP  (clear this to wake redistributor)" :
			"PE marked AWAKE");

	kprintf("ChildrenAsleep : %u (%s)\n", snap.children_asleep,
		snap.children_asleep ?
			"Children ASLEEP   (poll until 0 after clearing ProcessorSleep)" :
			"Children AWAKE    (safe to receive interrupts)");

	/* Warn explicitly if in a bad state for interrupt bring-up */
	if (snap.processor_sleep || snap.children_asleep) {
		kprintf("*** WARNING: Redistributor is NOT ready — interrupts will not be delivered!\n");
	} else {
		kprintf("Status      : Redistributor READY (both bits clear)\n");
	}

	kprintf("Raw         : 0x%x\n", snap.raw);
	kprintf("-----------------------------------------\n\n");
}

/**
 * @brief Dump and decode GICR_ISENABLER0 for the given PE.
 *
 * Prints the enable state of all 32 SGI and PPI INTIDs. SGI bits
 * [15:0] should always read as 1 (RAO). PPI bits [31:16] reflect
 * the current software-configured enable state.
 *
 * @param cpu_num The redistributor index (0-based PE number).
 */
static inline void print_gicr_isenabler0(const uint32_t cpu_num)
{
	gicr_isenabler0_t gicr_isenabler0;
	gicr_isenabler0.raw = get_gicr_isenabler0(cpu_num)->raw;

	kprintf("------- GICR_ISENABLER0 (cpu %u) --------\n", cpu_num);

	/* Decode each SGI bit individually */
	for (uint32_t i = 0; i < 16; i++) {
		uint32_t enabled = (gicr_isenabler0.sgi_enable >> i) & 1;
		kprintf("  SGI INTID %u : %s\n", i,
			enabled ? "ENABLED" : "disabled");
	}

	/* Decode each PPI bit individually */
	for (uint32_t i = 0; i < 16; i++) {
		uint32_t intid = 16 + i;
		uint32_t enabled = (gicr_isenabler0.ppi_enable >> i) & 1;
		kprintf("  PPI INTID %u : %s\n", intid,
			enabled ? "ENABLED" : "disabled");
	}

	kprintf("Raw         : 0x%x\n", gicr_isenabler0.raw);
	kprintf("-----------------------------------------\n\n");
}

/**
 * @brief Dump and decode GICR_IGROUPR0 and GICR_IGRPMODR0 together.
 *
 * Decodes the combined group assignment for every SGI and PPI by
 * reading both registers and resolving the group for each INTID.
 *
 * @param cpu_num The redistributor index (0-based PE number).
 */
static inline void print_gicr_igroup(const uint32_t cpu_num)
{
	gicr_igroupr0_t grp;
	gicr_igrpmodr0_t mod;
	grp.raw = get_gicr_igroupr0(cpu_num)->raw;
	mod.raw = get_gicr_igrpmodr0(cpu_num)->raw;

	kprintf("------- GICR_IGROUPR0/IGRPMODR0 (cpu %u) ---\n", cpu_num);

	for (uint32_t i = 0; i < 32; i++) {
		uint32_t t_grp = (grp.raw >> i) & 1;
		uint32_t t_grp_mod = (mod.raw >> i) & 1;

		const char *group_str;
		if (!t_grp && !t_grp_mod)
			group_str = "Group 0";
		else if (t_grp && !t_grp_mod)
			group_str = "Group 1 Non-secure";
		else if (!t_grp && t_grp_mod)
			group_str = "Group 1 Secure";
		else
			group_str = "Reserved";

		const char *type_str = i < 16 ? "SGI" : "PPI";
		kprintf("  %s INTID %u : %s\n", type_str, i, group_str);
	}

	kprintf("IGROUPR0  Raw : 0x%x\n", grp.raw);
	kprintf("IGRPMODR0 Raw : 0x%x\n", mod.raw);
	kprintf("---------------------------------------------\n\n");
}

/**
 * @brief Dump and decode all GICR_IPRIORITYR registers for the given PE.
 *
 * Prints the configured priority value for every SGI and PPI (INTIDs 0-31).
 *
 * @param cpu_num The redistributor index (0-based PE number).
 */
static inline void print_gicr_ipriorityr(const uint32_t cpu_num)
{
	kprintf("------- GICR_IPRIORITYR (cpu %u) --------\n", cpu_num);

	for (uint32_t i = 0; i < GICR_IPRIORITYR_COUNT; i++) {
		gicr_ipriorityr_t snap;
		snap.raw = get_gicr_ipriorityr(cpu_num, i)->raw;

		uint32_t base_intid = i * 4;
		kprintf("  INTID %u : 0x%x\n", base_intid + 0,
			(uint32_t)snap.prio_0);
		kprintf("  INTID %u : 0x%x\n", base_intid + 1,
			(uint32_t)snap.prio_1);
		kprintf("  INTID %u : 0x%x\n", base_intid + 2,
			(uint32_t)snap.prio_2);
		kprintf("  INTID %u : 0x%x\n", base_intid + 3,
			(uint32_t)snap.prio_3);
	}

	kprintf("-----------------------------------------\n\n");
}

/**
 * @brief Dump and decode MPIDR_EL1 for the current PE.
 *
 * Decodes the affinity hierarchy and flags. The AffinityValue field
 * (Aff3.Aff2.Aff1.Aff0) can be compared directly against
 * GICR_TYPER.AffinityValue to locate this PE's Redistributor frame.
 *
 * Fields:
 *  - Aff0          : Thread or core ID within a cluster
 *  - Aff1          : Cluster ID
 *  - Aff2          : Higher-level cluster / socket ID
 *  - Aff3          : Highest-level affinity (NUMA node etc.)
 *  - MT            : Multithreading — Aff0 is thread ID when set
 *  - U             : Uniprocessor — single PE, no affinity structure
 */
static inline void print_mpidr_el1(void)
{
	mpidr_el1_t mpidr;
	READ_SYS_REG(mpidr_el1, mpidr.raw);

	kprintf("-------------- MPIDR_EL1 ----------------\n");

	kprintf("Aff0        : %u (%s)\n", mpidr.aff0,
		mpidr.mt ? "Thread ID within core (MT==1)" :
			   "Core ID within cluster");

	kprintf("Aff1        : %u (Cluster ID)\n", mpidr.aff1);
	kprintf("Aff2        : %u (Higher cluster ID)\n", mpidr.aff2);
	kprintf("Aff3        : %u (Top-level affinity)\n", mpidr.aff3);

	kprintf("AffinityVal : Aff3=0x%x Aff2=0x%x Aff1=0x%x Aff0=0x%x"
		" (match against GICR_TYPER.AffinityValue)\n",
		mpidr.aff3, mpidr.aff2, mpidr.aff1, mpidr.aff0);

	kprintf("MT          : %u (%s)\n", mpidr.mt,
		mpidr.mt ? "Aff0 is THREAD ID (SMT system)" :
			   "Aff0 is CORE ID (no SMT)");

	kprintf("U           : %u (%s)\n", mpidr.uniprocessor,
		mpidr.uniprocessor ?
			"Uniprocessor (single PE, affinity fields meaningless)" :
			"Multiprocessor (affinity fields valid)");

	kprintf("Raw         : 0x%lx\n", mpidr.raw);
	kprintf("-----------------------------------------\n\n");
}

/**
 * @brief Dump and decode ICC_SRE_EL1 for the current PE.
 */
static inline void print_icc_sre_el1(void)
{
	icc_sre_el1_t icc_sre_el1;
	READ_SYS_REG(icc_sre_el1, icc_sre_el1.raw);

	kprintf("-------------- ICC_SRE_EL1 --------------\n");

	kprintf("SRE         : %u (%s)\n", icc_sre_el1.sre,
		icc_sre_el1.sre ?
			"System registers ENABLED (ICC_* accessible)" :
			"System registers DISABLED (GICC memory-map used)");

	kprintf("DFB         : %u (%s)\n", icc_sre_el1.dfb,
		icc_sre_el1.dfb ? "FIQ bypass DISABLED" : "FIQ bypass ENABLED");

	kprintf("DIB         : %u (%s)\n", icc_sre_el1.dib,
		icc_sre_el1.dib ? "IRQ bypass DISABLED" : "IRQ bypass ENABLED");

	if (!icc_sre_el1.sre) {
		kprintf("*** WARNING: SRE==0, all other ICC_* register accesses will FAULT!\n");
	}

	kprintf("Raw         : 0x%lx\n", icc_sre_el1.raw);
	kprintf("-----------------------------------------\n\n");
}

/**
 * @brief Dump and decode ICC_CTLR_EL1 for the current PE.
 */
static inline void print_icc_ctlr_el1(void)
{
	icc_ctlr_el1_t icc_ctlr_el1;
	READ_SYS_REG(icc_ctlr_el1, icc_ctlr_el1.raw);

	/* Decode number of priority bits: PRIbits+1 implemented bits */
	uint32_t num_pri_bits = icc_ctlr_el1.pri_bits + 1;
	uint32_t num_pri_levels = 1U << num_pri_bits;

	// NOLINTBEGIN(*-nested-conditional-operator)
	/* Decode INTID width */
	const char *id_bits_str =
		icc_ctlr_el1.id_bits == 0 ? "16-bit INTIDs (0-65535)" :
		icc_ctlr_el1.id_bits == 1 ? "24-bit INTIDs (0-16777215)" :
					    "Reserved";
	// NOLINTEND(*-nested-conditional-operator)

	kprintf("-------------- ICC_CTLR_EL1 -------------\n");

	kprintf("EOImode     : %u (%s)\n", icc_ctlr_el1.eoi_mode,
		icc_ctlr_el1.eoi_mode ?
			"Split mode: EOIR=priority drop only, DIR=deactivate" :
			"Combined mode: EOIR=priority drop AND deactivate");

	kprintf("CBPR        : %u (%s)\n", icc_ctlr_el1.cbpr,
		icc_ctlr_el1.cbpr ? "BPR0 used for both Group 0 and Group 1" :
				    "BPR0 and BPR1 are independent");

	kprintf("PRIbits     : %u (%u priority bits implemented = %u levels)\n",
		icc_ctlr_el1.pri_bits, num_pri_bits, num_pri_levels);

	kprintf("IDbits      : %u (%s)\n", icc_ctlr_el1.id_bits, id_bits_str);

	kprintf("SEIS        : %u (%s)\n", icc_ctlr_el1.seis,
		icc_ctlr_el1.seis ? "SEI generation SUPPORTED" :
				    "SEI generation NOT supported");

	kprintf("A3V         : %u (%s)\n", icc_ctlr_el1.a3v,
		icc_ctlr_el1.a3v ?
			"Non-zero Aff3 in SGI routing SUPPORTED" :
			"Non-zero Aff3 in SGI routing NOT supported");

	kprintf("Raw         : 0x%lx\n", icc_ctlr_el1.raw);
	kprintf("-----------------------------------------\n\n");
}

/**
 * @brief Dump and decode ICC_PMR_EL1 for the current PE.
 */
static inline void print_icc_pmr_el1(void)
{
	icc_pmr_el1_t icc_pmr_el1;
	READ_SYS_REG(icc_pmr_el1, icc_pmr_el1.raw);

	kprintf("-------------- ICC_PMR_EL1 --------------\n");

	kprintf("Priority    : 0x%x (%u)\n", icc_pmr_el1.priority,
		icc_pmr_el1.priority);

	if (icc_pmr_el1.priority == 0x00) {
		kprintf("Effect      : ALL interrupts MASKED (nothing will be delivered)\n");
	} else if (icc_pmr_el1.priority == 0xFF) {
		kprintf("Effect      : ALL interrupts UNMASKED (lowest priority threshold)\n");
	} else {
		kprintf("Effect      : Interrupts with priority < 0x%x will be delivered\n",
			icc_pmr_el1.priority);
	}

	kprintf("Raw         : 0x%lx\n", icc_pmr_el1.raw);
	kprintf("-----------------------------------------\n\n");
}

/**
 * @brief Dump and decode ICC_BPR1_EL1 for the current PE.
 */
static inline void print_icc_bpr1_el1(void)
{
	icc_bpr1_el1_t icc_bpr1_el1;
	READ_SYS_REG(icc_bpr1_el1, icc_bpr1_el1.raw);

	/* Group field width = 7 - BinaryPoint, subpriority width =
	 * BinaryPoint+1 */
	uint32_t group_bits = 7 - icc_bpr1_el1.binary_point;
	uint32_t sub_bits = icc_bpr1_el1.binary_point + 1;

	kprintf("-------------- ICC_BPR1_EL1 -------------\n");

	kprintf("BinaryPoint : %u\n", icc_bpr1_el1.binary_point);
	kprintf("Effect      : Priority[7:%u] = preemption group (%u bits)\n",
		icc_bpr1_el1.binary_point + 1, group_bits);
	kprintf("              Priority[%u:0] = subpriority     (%u bits, no preemption)\n",
		icc_bpr1_el1.binary_point, sub_bits);

	kprintf("Raw         : 0x%lx\n", icc_bpr1_el1.raw);
	kprintf("-----------------------------------------\n\n");
}

/**
 * @brief Dump and decode ICC_IGRPEN1_EL1 for the current PE.
 */
static inline void print_icc_igrpen1_el1(void)
{
	icc_igrpen1_el1_t icc_igrpen1_el1;
	READ_SYS_REG(icc_igrpen1_el1, icc_igrpen1_el1.raw);

	kprintf("-------------- ICC_IGRPEN1_EL1 ----------\n");

	kprintf("Enable      : %u (%s)\n", icc_igrpen1_el1.enable,
		icc_igrpen1_el1.enable ?
			"Group 1 interrupts ENABLED on this PE" :
			"Group 1 interrupts DISABLED on this PE");

	if (!icc_igrpen1_el1.enable) {
		kprintf("*** WARNING: Group 1 interrupts will not be delivered to this PE!\n");
	}

	kprintf("Raw         : 0x%lx\n", icc_igrpen1_el1.raw);
	kprintf("-----------------------------------------\n\n");
}

/**
 * @brief Enable Group 1 NS interrupts and affinity routing on the distributor.
 */
static inline void set_up_gicd_ctrl(void)
{
	gicd_ctlr_t gicd_ctlr;
	gicd_ctlr.raw = get_gicd_ctlr()->raw;

	gicd_ctlr.enable_grp1ns = 1;
	gicd_ctlr.are_ns = 1;
	gicd_ctlr.are_s = 1;

	get_gicd_ctlr()->raw = gicd_ctlr.raw;
	while (get_gicd_ctlr()->rwp != 0) {
	}
	kprintf("Raw         : 0x%x\n", get_gicd_ctlr()->raw);
}

/**
 * @brief Wake the redistributor for a PE and wait until it acknowledges.
 *
 * Clears ProcessorSleep in GICR_WAKER and polls ChildrenAsleep until the
 * hardware reports the CPU interface is awake.
 *
 * @param cpu_num The redistributor index (0-based PE number).
 */
static inline void set_gicr_waker(uint32_t cpu_num)
{
	print_gicr_waker(cpu_num);

	gicr_waker_t gicr_waker;
	gicr_waker.raw = get_gicr_waker(cpu_num)->raw;
	gicr_waker.processor_sleep = 0;
	get_gicr_waker(cpu_num)->raw = gicr_waker.raw;

	print_gicr_waker(cpu_num);

	/* Poll ChildrenAsleep until it clears — hardware sets this to 0
	 * once all CPU interface children have acknowledged the wakeup */
	while (get_gicr_waker(cpu_num)->children_asleep != 0) {
		;
	}
}

/**
 * @brief Set ICC_SRE_EL1.SRE to enable the System Register interface for
 * the CPU interface.
 */
static inline void enable_icc_sys_reg_interface(void)
{
	icc_sre_el1_t icc_sre_el1;
	READ_SYS_REG(icc_sre_el1, icc_sre_el1.raw);

	icc_sre_el1.sre = 1;

	WRITE_SYS_REG(icc_sre_el1, icc_sre_el1.raw);
}

/**
 * @brief Set the priority mask threshold for the current PE.
 *
 * Only interrupts with a priority numerically LESS THAN this value
 * will be delivered to the PE.
 *
 * @param threshold  0x00 = mask all interrupts.
 *                   0xFF = allow all interrupts through (lowest threshold).
 */
static inline void set_core_priority_threshold(uint8_t threshold)
{
	icc_pmr_el1_t icc_pmr_el1;
	icc_pmr_el1.raw = 0;

	icc_pmr_el1.priority = threshold;

	WRITE_SYS_REG(icc_pmr_el1, icc_pmr_el1.raw);
}

/**
 * @brief Unmask all interrupts on the current PE.
 *
 * Sets ICC_PMR_EL1 to 0xFF, the lowest possible priority threshold,
 * allowing interrupts of any priority to be delivered to this PE.
 *
 * @note The GIC Distributor (GICD_CTLR) and Redistributor (GICR_CTLR)
 *       group enables must also be configured for interrupts to actually
 *       arrive. This is a necessary but not sufficient condition.
 */
static inline void unmask_all_core_interrupts(void)
{
	set_core_priority_threshold(0xFF);
}

/**
 * @brief Mask all interrupts on the current PE.
 *
 * Sets ICC_PMR_EL1 to 0x00, the highest possible priority threshold.
 * No interrupt priority can be numerically less than 0, so nothing
 * is delivered to this PE regardless of other GIC configuration.
 *
 * @note This only masks interrupt delivery to this PE via the priority
 *       threshold mechanism. It does not affect the pending state of
 *       interrupts in the Distributor or Redistributor.
 */
static inline void mask_all_core_interrupts(void)
{
	set_core_priority_threshold(0x00);
}

/**
 * @brief Configure ICC_BPR1_EL1 for the current PE.
 *
 * Sets the binary point register for Group 1 interrupts, which controls
 * the split between the preemption group priority and subpriority fields.
 *
 * The split is defined as:
 *   - Priority[7:(BinaryPoint+1)] = group priority (controls preemption)
 *   - Priority[BinaryPoint:0]     = subpriority    (no preemption effect)
 *
 * A value of 0 means Priority[7:1] is the group priority and Priority[0]
 * is subpriority — giving near-maximum preemption granularity. This is
 * the correct setting for a simple bare-metal kernel.
 *
 * @param binary_point  The binary point split value (0–7).
 *                      0 = Priority[7:1] group, Priority[0] subpriority.
 *                      7 = all bits are subpriority (no preemption at all).
 *                      Minimum value is implementation-defined; read
 *                      ICC_CTLR_EL1.PRIbits to determine the floor.
 */
static inline void set_core_binary_point(uint8_t binary_point)
{
	icc_bpr1_el1_t icc_bpr1_el1;
	icc_bpr1_el1.raw = 0;
	icc_bpr1_el1.binary_point = binary_point & 0x7;
	WRITE_SYS_REG(icc_bpr1_el1, icc_bpr1_el1.raw);
}

/**
 * @brief Configure ICC_BPR1_EL1 to the minimum allowed value for this PE.
 *
 * The GICv3 spec mandates a minimum binary point value of (7 - PRIbits),
 * where PRIbits is the number of implemented priority bits read from
 * ICC_CTLR_EL1. Writing any value below this minimum is silently clamped
 * by hardware back up to the minimum.
 */
static inline void set_core_min_binary_point(void)
{
	icc_ctlr_el1_t icc_ctlr_el1;
	READ_SYS_REG(icc_ctlr_el1, icc_ctlr_el1.raw);
	const uint32_t min_bpr = 7 - icc_ctlr_el1.pri_bits;
	set_core_binary_point((uint8_t)min_bpr);
}

/**
 * @brief Enable combined EOI mode on the current PE.
 *
 * Sets ICC_CTLR_EL1.EOImode = 0, which means a single write to
 * ICC_EOIR1_EL1 performs both priority drop AND deactivation atomically.
 *
 * This is the default and simplest EOI mode, suitable for bare-metal
 * kernels that do not require re-enabling preemption between priority
 * drop and deactivation.
 *
 * @note If split EOI mode (EOImode = 1) is needed, use
 *       enable_split_eoi() instead, and pair ICC_EOIR1_EL1 writes
 *       with separate ICC_DIR_EL1 deactivation writes.
 */
static inline void set_combined_eoi_mode(void)
{
	icc_ctlr_el1_t icc_ctlr_el1;
	READ_SYS_REG(icc_ctlr_el1, icc_ctlr_el1.raw);
	icc_ctlr_el1.eoi_mode = 0;
	WRITE_SYS_REG(icc_ctlr_el1, icc_ctlr_el1.raw);
}

/**
 * @brief Enable Group 1 interrupt forwarding on the current PE.
 *
 * Sets ICC_IGRPEN1_EL1.Enable = 1, which allows Group 1 interrupts to
 * be signaled to this PE by the CPU interface.
 */
static inline void enable_grp1_interrupts(void)
{
	icc_igrpen1_el1_t icc_igrpen1_el1;
	READ_SYS_REG(icc_igrpen1_el1, icc_igrpen1_el1.raw);
	icc_igrpen1_el1.enable = 1;
	WRITE_SYS_REG(icc_igrpen1_el1, icc_igrpen1_el1.raw);
}

/**
 * @brief Map a contiguous physical memory region to a virtual address range.
 *
 * Maps every 4KB page in [p_base, p_base+size) to the corresponding
 * virtual address starting at v_base, applying the specified permissions
 * to every page.
 *
 * For identity mapping, pass p_base == v_base. For high-memory mapping,
 * pass v_base = pa_to_va(p_base).
 *
 * @param p_base  Physical base address (must be 4KB aligned).
 * @param v_base  Virtual base address to map to (must be 4KB aligned).
 * @param size    Size in bytes (must be non-zero; rounded up to 4KB if needed).
 * @param perms   Page permissions to apply to every mapped page.
 * @param mem_type  Memory type index (@ref mem_type_t) written into
 * AttrIndx[2:0] of the leaf descriptor, selecting the MAIR_EL1 attribute byte
 *                 (e.g. @ref device → 0x00 Device-nGnRnE,
 *                       @ref normal → 0xFF Normal WB-RWA Cacheable).
 * @return true on success, false if alignment checks fail or any page
 *         mapping fails.
 */
static inline bool map_continuous(phy_addr p_base, virt_addr v_base,
				  size_t size, page_permissions_t perms,
				  enum mem_type_t mem_type)
{
	if (p_base % PAGE_SIZE != 0) {
		kprintf("Error: p_base 0x%lx is not 4KB aligned\n", p_base);
		return false;
	}
	if (v_base % PAGE_SIZE != 0) {
		kprintf("Error: v_base 0x%lx is not 4KB aligned\n", v_base);
		return false;
	}

	if (size == 0) {
		kprintf("Error: map_continuous called with size 0\n");
		return false;
	}

	if (size % PAGE_SIZE != 0) {
		kprintf("Warning: size 0x%lx is not page-aligned, rounding up\n",
			size);
		size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	}

	phy_addr p_start = p_base;
	virt_addr v_start = v_base;
	phy_addr end = p_base + size;

	while (p_start < end) {
		if (!map_page(get_kernel_map_root(), v_start, p_start, perms,
			      mem_type)) {
			kprintf("Error: failed to map page at 0x%lx\n",
				p_start);
			return false;
		}
		p_start += PAGE_SIZE;
		v_start += PAGE_SIZE;
	}

	return true;
}

/**
 * @brief Enable forwarding of a single SGI or PPI for the given PE.
 *
 * Sets the corresponding bit in GICR_ISENABLER0 for the given INTID.
 * Writing 0 has no effect — this is a set-enable register.
 *
 * @param cpu_num The redistributor index (0-based PE number).
 * @param intid   SGI or PPI INTID to enable (0-31).
 * @return true if successful, false if intid is out of range.
 */
static inline bool enable_sgi_ppi(const uint32_t cpu_num, const uint32_t intid)
{
	if (intid > 31) {
		kprintf("Error: invalid SGI/PPI INTID %u — must be 0-31\n",
			intid);
		return false;
	}

	get_gicr_isenabler0(cpu_num)->raw = (1U << intid);
	return true;
}

/**
 * @brief Assign a single SGI or PPI to Group 1 Non-secure for the given PE.
 *
 * Sets the corresponding bit in GICR_IGROUPR0 and clears the
 * corresponding bit in GICR_IGRPMODR0, configuring the INTID as
 * Group 1 Non-secure.
 *
 * @param cpu_num The redistributor index (0-based PE number).
 * @param intid   SGI or PPI INTID to configure (0-31).
 * @return true if successful, false if intid is out of range.
 */
static inline bool set_sgi_ppi_group1_ns(const uint32_t cpu_num,
					 const uint32_t intid)
{
	if (intid > 31) {
		kprintf("Error: invalid SGI/PPI INTID %u — must be 0-31\n",
			intid);
		return false;
	}

	gicr_igroupr0_t grp;
	grp.raw = get_gicr_igroupr0(cpu_num)->raw;
	grp.raw |= (1U << intid);
	get_gicr_igroupr0(cpu_num)->raw = grp.raw;

	gicr_igrpmodr0_t mod;
	mod.raw = get_gicr_igrpmodr0(cpu_num)->raw;
	mod.raw &= ~(1U << intid);
	get_gicr_igrpmodr0(cpu_num)->raw = mod.raw;

	return true;
}

/**
 * @brief Set the priority of a single SGI or PPI for the given PE.
 *
 * Writes the priority byte into the correct byte lane of the appropriate
 * GICR_IPRIORITYR register for the given INTID, leaving other INTIDs
 * in the same register unaffected.
 *
 * The priority must be numerically less than ICC_PMR_EL1 for the
 * interrupt to be delivered. Only the high bits are implemented
 * (determined by ICC_CTLR_EL1.PRIbits); unimplemented low bits are
 * RAZ/WI and will be masked by hardware.
 *
 * @param cpu_num  The redistributor index (0-based PE number).
 * @param intid    SGI or PPI INTID to configure (0-31).
 * @param priority Priority value to assign (e.g. 0xA0).
 * @return true if successful, false if intid is out of range.
 */
static inline bool set_sgi_ppi_priority(const uint32_t cpu_num,
					const uint32_t intid,
					const uint8_t priority)
{
	if (intid > 31) {
		kprintf("Error: invalid SGI/PPI INTID %u — must be 0-31\n",
			intid);
		return false;
	}

	uint32_t reg_idx = intid / 4; /* which IPRIORITYR register */
	uint32_t byte_idx = intid % 4; /* which byte within that register */
	uint32_t shift = byte_idx * 8;
	uint32_t mask = 0xFFU << shift;

	gicr_ipriorityr_t gicr_ipriorityr;
	gicr_ipriorityr.raw = get_gicr_ipriorityr(cpu_num, reg_idx)->raw;
	gicr_ipriorityr.raw = (gicr_ipriorityr.raw & ~mask) |
			      ((uint32_t)priority << shift);
	get_gicr_ipriorityr(cpu_num, reg_idx)->raw = gicr_ipriorityr.raw;

	return true;
}

/**
 * @brief Configure a single SGI or PPI for Group 1 NS delivery.
 *
 * Convenience wrapper that calls all three configuration steps for a
 * single SGI or PPI INTID in the correct order:
 *   1. Set Group 1 Non-secure assignment (GICR_IGROUPR0 / GICR_IGRPMODR0)
 *   2. Set priority                      (GICR_IPRIORITYR)
 *   3. Enable forwarding                 (GICR_ISENABLER0)
 *
 * After this call the INTID is fully configured and will be delivered
 * to the CPU interface as a Group 1 NS interrupt provided the GIC
 * distributor, redistributor, and CPU interface are also enabled.
 *
 * @param cpu_num  The redistributor index (0-based PE number).
 * @param intid    SGI or PPI INTID to configure (0-31).
 * @param priority Priority value to assign (e.g. 0xA0).
 * @return true if all steps succeeded, false if intid is out of range.
 */
static inline bool configure_sgi_ppi(const uint32_t cpu_num,
				     const uint32_t intid,
				     const uint8_t priority)
{
	if (!set_sgi_ppi_group1_ns(cpu_num, intid)) {
		return false;
	}

	if (!set_sgi_ppi_priority(cpu_num, intid, priority)) {
		return false;
	}

	if (!enable_sgi_ppi(cpu_num, intid)) {
		return false;
	}

	return true;
}

/**
 * @brief Enable forwarding of a single SPI for delivery.
 *
 * Sets the corresponding bit in GICD_ISENABLER<n> for the given SPI INTID.
 * Writing 0 has no effect — this is a set-enable register.
 *
 * @param intid SPI INTID to enable (must be >= 32).
 * @return true if successful, false if intid is out of SPI range.
 */
static inline bool enable_spi(const uint32_t intid)
{
	if (intid < 32) {
		kprintf("Error: INTID %u is not an SPI — SPIs start at INTID 32\n",
			intid);
		return false;
	}

	uint32_t reg_idx = intid / 32;
	uint32_t bit_idx = intid % 32;
	get_gicd_isenabler(reg_idx)->raw = (1U << bit_idx);
	return true;
}

/**
 * @brief Assign a single SPI to Group 1 Non-secure.
 *
 * Sets the corresponding bit in GICD_IGROUPR<n> and clears the
 * corresponding bit in GICD_IGRPMODR<n>, configuring the SPI as
 * Group 1 Non-secure so it can be received via ICC_IAR1_EL1.
 *
 * @param intid SPI INTID to configure (must be >= 32).
 * @return true if successful, false if intid is out of SPI range.
 */
static inline bool set_spi_group1_ns(const uint32_t intid)
{
	if (intid < 32) {
		kprintf("Error: INTID %u is not an SPI — SPIs start at INTID 32\n",
			intid);
		return false;
	}

	uint32_t reg_idx = intid / 32;
	uint32_t bit_idx = intid % 32;

	/* Read-modify-write to avoid disturbing other INTIDs */
	gicd_igroupr_t grp;
	grp.raw = get_gicd_igroupr(reg_idx)->raw;
	grp.raw |= (1U << bit_idx);
	get_gicd_igroupr(reg_idx)->raw = grp.raw;

	gicd_igrpmodr_t mod;
	mod.raw = get_gicd_igrpmodr(reg_idx)->raw;
	mod.raw &= ~(1U << bit_idx);
	get_gicd_igrpmodr(reg_idx)->raw = mod.raw;

	return true;
}

/**
 * @brief Set the priority of a single SPI.
 *
 * Writes the priority byte into the correct byte lane of the appropriate
 * GICD_IPRIORITYR register for the given SPI INTID, leaving other INTIDs
 * in the same register unaffected.
 *
 * @param intid    SPI INTID to configure (must be >= 32).
 * @param priority Priority value to assign (e.g. 0xA0).
 * @return true if successful, false if intid is out of SPI range.
 */
static inline bool set_spi_priority(const uint32_t intid,
				    const uint8_t priority)
{
	if (intid < 32) {
		kprintf("Error: INTID %u is not an SPI — SPIs start at INTID 32\n",
			intid);
		return false;
	}

	uint32_t reg_idx = intid / 4;
	uint32_t byte_idx = intid % 4;
	uint32_t shift = byte_idx * 8;
	uint32_t mask = 0xFFU << shift;

	gicd_ipriorityr_t snap;
	snap.raw = get_gicd_ipriorityr(reg_idx)->raw;
	snap.raw = (snap.raw & ~mask) | ((uint32_t)priority << shift);
	get_gicd_ipriorityr(reg_idx)->raw = snap.raw;

	return true;
}

/**
 * @brief Route a single SPI to a specific PE via affinity.
 *
 * Writes GICD_IROUTER<intid> to route the SPI to the PE identified
 * by the given affinity values. On a single-core system pass all
 * zeros to route to PE 0.0.0.0.
 *
 * @param intid SPI INTID to route (must be >= 32).
 * @param aff0  Target Aff0 value.
 * @param aff1  Target Aff1 value.
 * @param aff2  Target Aff2 value.
 * @param aff3  Target Aff3 value.
 * @return true if successful, false if intid is out of SPI range.
 */
static inline bool set_spi_route(const uint32_t intid, const uint32_t aff0,
				 const uint32_t aff1, const uint32_t aff2,
				 const uint32_t aff3)
{
	if (intid < 32) {
		kprintf("Error: INTID %u is not an SPI — SPIs start at INTID 32\n",
			intid);
		return false;
	}

	gicd_irouter_t router;
	router.raw = 0;
	router.aff0 = aff0 & 0xFF;
	router.aff1 = aff1 & 0xFF;
	router.aff2 = aff2 & 0xFF;
	router.aff3 = aff3 & 0xFF;
	router.irm = GICD_IRM_AFFINITY;
	get_gicd_irouter(intid)->raw = router.raw;

	return true;
}

/**
 * @brief Configure a single SPI for Group 1 NS delivery to a specific PE.
 *
 * Convenience wrapper that calls all four configuration steps for a
 * single SPI INTID in the correct order:
 *   1. Set Group 1 Non-secure assignment (GICD_IGROUPR / GICD_IGRPMODR)
 *   2. Set priority                      (GICD_IPRIORITYR)
 *   3. Set affinity routing              (GICD_IROUTER)
 *   4. Enable forwarding                 (GICD_ISENABLER)
 *
 * @param intid    SPI INTID to configure (must be >= 32).
 * @param priority Priority value to assign (e.g. 0xA0).
 * @param aff0     Target PE Aff0.
 * @param aff1     Target PE Aff1.
 * @param aff2     Target PE Aff2.
 * @param aff3     Target PE Aff3.
 * @return true if all steps succeeded, false if intid is out of SPI range.
 */
static inline bool configure_spi(const uint32_t intid, const uint8_t priority,
				 const uint32_t aff0, const uint32_t aff1,
				 const uint32_t aff2, const uint32_t aff3)
{
	if (!set_spi_group1_ns(intid)) {
		return false;
	}
	if (!set_spi_priority(intid, priority)) {
		return false;
	}
	if (!set_spi_route(intid, aff0, aff1, aff2, aff3)) {
		return false;
	}
	if (!enable_spi(intid)) {
		return false;
	}
	return true;
}

/**
 * @brief Unmask all exceptions on the current PE.
 *
 * Clears all four bits (D, A, I, F) in DAIF simultaneously.
 * Only call this once valid handlers are installed for all exception
 * types in the vector table.
 */
static inline void unmask_all_exceptions(void)
{
	asm volatile("msr daifclr, #0xf" ::: "memory");
	asm volatile("isb");
}

/**
 * @brief Mask all exceptions on the current PE.
 *
 * Sets all four bits (D, A, I, F) in DAIF simultaneously.
 * Used to enter a fully non-interruptible critical section.
 */
static inline void mask_all_exceptions(void)
{
	asm volatile("msr daifset, #0xf" ::: "memory");
	asm volatile("isb");
}

/* ── SGI / PPI (Redistributor) ───────────────────────────────────────────── */

/**
 * @brief Get GICR_ICENABLER0 register (SGI frame, offset 0x0180, RW).
 *
 * Clear-Enable register controlling SGI and PPI interrupt forwarding for
 * this PE. Writing 1 to a bit disables the corresponding INTID; writing
 * 0 has no effect.
 *
 * @param cpu_num The redistributor index (0-based PE number).
 * @return Volatile pointer to GICR_ICENABLER0 for this PE.
 */
static inline volatile gicr_isenabler0_t *
get_gicr_icenabler0(const uint32_t cpu_num)
{
	// NOLINTNEXTLINE(*int-to-ptr)
	return (volatile gicr_isenabler0_t *)(get_gicr_sgi_base(cpu_num) +
					      0x0180);
}

/**
 * @brief Disable forwarding of a single SGI or PPI for the given PE.
 *
 * Sets the corresponding bit in GICR_ICENABLER0 for the given INTID.
 * Writing 1 disables the interrupt. Writing 0 has no effect.
 *
 * @param cpu_num The redistributor index (0-based PE number).
 * @param intid   SGI or PPI INTID to disable (0-31).
 * @return true if successful, false if intid is out of range.
 */
static inline bool disable_sgi_ppi(const uint32_t cpu_num, const uint32_t intid)
{
	if (intid > 31) {
		kprintf("Error: invalid SGI/PPI INTID %u — must be 0-31\n",
			intid);
		return false;
	}

	get_gicr_icenabler0(cpu_num)->raw = (1U << intid);
	return true;
}

/**
 * @brief Assign a single SGI or PPI to Group 0 for the given PE.
 *
 * Clears the corresponding bit in GICR_IGROUPR0 and clears the
 * corresponding bit in GICR_IGRPMODR0, configuring the INTID as
 * Group 0 (the reset default).
 *
 * @param cpu_num The redistributor index (0-based PE number).
 * @param intid   SGI or PPI INTID to unconfigure (0-31).
 * @return true if successful, false if intid is out of range.
 */
static inline bool clear_sgi_ppi_group(const uint32_t cpu_num,
				       const uint32_t intid)
{
	if (intid > 31) {
		kprintf("Error: invalid SGI/PPI INTID %u — must be 0-31\n",
			intid);
		return false;
	}

	gicr_igroupr0_t grp;
	grp.raw = get_gicr_igroupr0(cpu_num)->raw;
	grp.raw &= ~(1U << intid);
	get_gicr_igroupr0(cpu_num)->raw = grp.raw;

	gicr_igrpmodr0_t mod;
	mod.raw = get_gicr_igrpmodr0(cpu_num)->raw;
	mod.raw &= ~(1U << intid);
	get_gicr_igrpmodr0(cpu_num)->raw = mod.raw;

	return true;
}

/**
 * @brief Reset the priority of a single SGI or PPI to 0 for the given PE.
 *
 * Clears the priority byte in the appropriate GICR_IPRIORITYR register
 * for the given INTID, leaving other INTIDs in the same register unaffected.
 *
 * @param cpu_num The redistributor index (0-based PE number).
 * @param intid   SGI or PPI INTID to unconfigure (0-31).
 * @return true if successful, false if intid is out of range.
 */
static inline bool clear_sgi_ppi_priority(const uint32_t cpu_num,
					  const uint32_t intid)
{
	if (intid > 31) {
		kprintf("Error: invalid SGI/PPI INTID %u — must be 0-31\n",
			intid);
		return false;
	}

	return set_sgi_ppi_priority(cpu_num, intid, 0x00);
}

/**
 * @brief Fully unconfigure a single SGI or PPI for the given PE.
 *
 * Reverses configure_sgi_ppi() by performing all teardown steps in the
 * correct order:
 *   1. Disable forwarding  (GICR_ICENABLER0)
 *   2. Reset priority to 0 (GICR_IPRIORITYR)
 *   3. Reset group to 0    (GICR_IGROUPR0 / GICR_IGRPMODR0)
 *
 * @param cpu_num The redistributor index (0-based PE number).
 * @param intid   SGI or PPI INTID to unconfigure (0-31).
 * @return true if all steps succeeded, false if intid is out of range.
 */
static inline bool unconfigure_sgi_ppi(const uint32_t cpu_num,
				       const uint32_t intid)
{
	if (!disable_sgi_ppi(cpu_num, intid)) {
		return false;
	}
	if (!clear_sgi_ppi_priority(cpu_num, intid)) {
		return false;
	}
	if (!clear_sgi_ppi_group(cpu_num, intid)) {
		return false;
	}
	return true;
}

/* ── SPI (Distributor) ──────────────────────────────────────────────────────
 */

/**
 * @brief Disable forwarding of a single SPI.
 *
 * Sets the corresponding bit in GICD_ICENABLER<n> for the given SPI INTID.
 * Writing 1 disables the interrupt. Writing 0 has no effect.
 *
 * @param intid SPI INTID to disable (must be >= 32).
 * @return true if successful, false if intid is out of SPI range.
 */
static inline bool disable_spi(const uint32_t intid)
{
	if (intid < 32) {
		kprintf("Error: INTID %u is not an SPI — SPIs start at INTID 32\n",
			intid);
		return false;
	}

	uint32_t reg_idx = intid / 32;
	uint32_t bit_idx = intid % 32;
	get_gicd_icenabler(reg_idx)->raw = (1U << bit_idx);
	return true;
}

/**
 * @brief Reset the Group assignment of a single SPI to Group 0.
 *
 * Clears the corresponding bits in both GICD_IGROUPR<n> and
 * GICD_IGRPMODR<n>, restoring the reset default of Group 0.
 *
 * @param intid SPI INTID to unconfigure (must be >= 32).
 * @return true if successful, false if intid is out of SPI range.
 */
static inline bool clear_spi_group(const uint32_t intid)
{
	if (intid < 32) {
		kprintf("Error: INTID %u is not an SPI — SPIs start at INTID 32\n",
			intid);
		return false;
	}

	uint32_t reg_idx = intid / 32;
	uint32_t bit_idx = intid % 32;

	gicd_igroupr_t grp;
	grp.raw = get_gicd_igroupr(reg_idx)->raw;
	grp.raw &= ~(1U << bit_idx);
	get_gicd_igroupr(reg_idx)->raw = grp.raw;

	gicd_igrpmodr_t mod;
	mod.raw = get_gicd_igrpmodr(reg_idx)->raw;
	mod.raw &= ~(1U << bit_idx);
	get_gicd_igrpmodr(reg_idx)->raw = mod.raw;

	return true;
}

/**
 * @brief Reset the priority of a single SPI to 0.
 *
 * Clears the priority byte in the appropriate GICD_IPRIORITYR register
 * for the given SPI INTID, leaving other INTIDs unaffected.
 *
 * @param intid SPI INTID to unconfigure (must be >= 32).
 * @return true if successful, false if intid is out of SPI range.
 */
static inline bool clear_spi_priority(const uint32_t intid)
{
	if (intid < 32) {
		kprintf("Error: INTID %u is not an SPI — SPIs start at INTID 32\n",
			intid);
		return false;
	}

	return set_spi_priority(intid, 0x00);
}

/**
 * @brief Reset the affinity routing of a single SPI to PE 0.0.0.0.
 *
 * Clears GICD_IROUTER<intid> to zero, which routes the SPI to PE
 * 0.0.0.0 using affinity routing mode (IRM=0).
 *
 * @param intid SPI INTID to unconfigure (must be >= 32).
 * @return true if successful, false if intid is out of SPI range.
 */
static inline bool clear_spi_route(const uint32_t intid)
{
	if (intid < 32) {
		kprintf("Error: INTID %u is not an SPI — SPIs start at INTID 32\n",
			intid);
		return false;
	}

	get_gicd_irouter(intid)->raw = 0;
	return true;
}

/**
 * @brief Fully unconfigure a single SPI.
 *
 * Reverses configure_spi() by performing all teardown steps in the
 * correct order:
 *   1. Disable forwarding  (GICD_ICENABLER)
 *   2. Reset priority to 0 (GICD_IPRIORITYR)
 *   3. Reset routing       (GICD_IROUTER)
 *   4. Reset group to 0    (GICD_IGROUPR / GICD_IGRPMODR)
 *
 * @param intid SPI INTID to unconfigure (must be >= 32).
 * @return true if all steps succeeded, false if intid is out of SPI range.
 */
static inline bool unconfigure_spi(const uint32_t intid)
{
	if (!disable_spi(intid)) {
		return false;
	}
	if (!clear_spi_priority(intid)) {
		return false;
	}
	if (!clear_spi_route(intid)) {
		return false;
	}
	if (!clear_spi_group(intid)) {
		return false;
	}
	return true;
}

void gicv3_init(const void *fdt)
{
	const int intc_node = get_intc_node_offset(fdt);
	if (fdt_is_error(intc_node)) {
		kprintf("No interrupt controller node found in FDT\n");
		return;
	}

	memory_map_t reg_map;
	if (!get_reg_property(fdt, intc_node, &reg_map)) {
		kprintf("Failed to retrieve reg property for interrupt controller\n");
		return;
	}

	kprintf("GICv3: Found %d register regions\n", reg_map.count);
	if (reg_map.count != 2) {
		kprintf("GICv3 Error: Expected exactly 2 reg regions for distributor and redistributor, found %d\n",
			reg_map.count);
		return;
	}

	for (int i = 0; i < reg_map.count; i++) {
		kprintf("Mapping ICU Reg Region %d: base=0x%lx, size=0x%lx\n",
			i, reg_map.regions[i].base, reg_map.regions[i].size);
		if (!map_continuous(reg_map.regions[i].base,
				    pa_to_va(reg_map.regions[i].base),
				    reg_map.regions[i].size,
				    (page_permissions_t){ .read = true,
							  .write = true,
							  .execute = false,
							  .user_accessible =
								  false },
				    device)) {
			kprintf("Failed to map ICU Reg Region %d: base=0x%lx, size=0x%lx\n",
				i, reg_map.regions[i].base,
				reg_map.regions[i].size);
		}
	}

	// Setup base addresses
	memap.dist_base = pa_to_va(reg_map.regions[0].base);
	memap.redist_base = pa_to_va(reg_map.regions[1].base);

	mask_all_exceptions();
	print_gicd_typer();
	print_gicd_iidr();
	set_up_gicd_ctrl();
	print_gicd_ctlr();
	print_mpidr_el1();
	print_gicr_waker(get_core_id());
	set_gicr_waker(get_core_id());
	print_gicr_waker(get_core_id());
	print_gicr_isenabler0(get_core_id());
	print_gicr_igroup(get_core_id());
	print_gicr_ipriorityr(get_core_id());

	print_current_el();
	enable_icc_sys_reg_interface();
	print_icc_sre_el1();
	mask_all_core_interrupts();
	unmask_all_core_interrupts();
	print_icc_pmr_el1();
	set_core_min_binary_point();
	print_icc_bpr1_el1();
	set_combined_eoi_mode();
	print_icc_ctlr_el1();
	enable_grp1_interrupts();
	print_icc_igrpen1_el1();
	unmask_all_exceptions();

	asm volatile("dsb sy\n\tisb" ::: "memory");
	kprintf("GICv3 initialization complete\n");
}

bool gicv3_register_irq(uint32_t irq_num, handler_data_t handler_data)
{
	if (irq_num >= MAX_INT_ID) {
		kprintf("Error: INTID %u is out of range (max %u)\n", irq_num,
			MAX_INT_ID - 1);
		return false;
	}

	if (irq_num < 32) {
		if (!configure_sgi_ppi(get_core_id(), irq_num, 0xA0)) {
			kprintf("Error: failed to configure SGI/PPI INTID %u\n",
				irq_num);
			return false;
		}
	} else {
		if (!configure_spi(irq_num, 0xA0, 0, 0, 0, 0)) {
			kprintf("Error: failed to configure SPI INTID %u\n",
				irq_num);
			return false;
		}
	}

	dispatch_table[irq_num] = handler_data;
	asm volatile("dsb sy" ::: "memory");
	return true;
}

void gicv3_unregister_irq(uint32_t irq_num)
{
	if (irq_num >= MAX_INT_ID) {
		kprintf("Error: INTID %u is out of range (max %u)\n", irq_num,
			MAX_INT_ID - 1);
		return;
	}

	if (irq_num < 32) {
		unconfigure_sgi_ppi(get_core_id(), irq_num);
	} else {
		unconfigure_spi(irq_num);
	}

	dispatch_table[irq_num] =
		(handler_data_t){ .handler = NULL, .private_data = NULL };
	asm volatile("dsb sy" ::: "memory");
}

void gicv3_handle_irq(void)
{
	/* 1. Acknowledge — moves interrupt from pending to active */
	icc_iar1_el1_t iar;
	READ_SYS_REG(icc_iar1_el1, iar.raw);
	uint32_t intid = (uint32_t)iar.intid;

	/* 2. Spurious interrupt check */
	if (intid == 1023 || intid == 1022) {
		kprintf("Spurious interrupt, ignoring\n");
		unmask_all_exceptions();
		return;
	}

	if (intid >= MAX_INT_ID) {
		kprintf("Error: out-of-range INTID %u received\n", intid);
		unmask_all_exceptions();
		return;
	}

	/* 3. Dispatch */
	const handler_data_t handler = dispatch_table[intid];
	if (handler.handler != NULL) {
		handler.handler(handler.private_data);
	} else {
		kprintf("Warning: no handler registered for INTID %u\n", intid);
	}

	/* 4. EOI — priority drop and deactivation (combined mode) */
	icc_eoir1_el1_t eoir;
	eoir.raw = 0;
	eoir.intid = intid;
	WRITE_SYS_REG(icc_eoir1_el1, eoir.raw);
}
