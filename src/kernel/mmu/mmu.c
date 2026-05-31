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

#include "utils/kprintf.h"
#include <stdint.h>

#define READ_SYS_REG(reg, val)                             \
	do {                                               \
		asm volatile("mrs %0, " #reg : "=r"(val)); \
	} while (0)

#define WRITE_SYS_REG(reg, val)                                \
	do {                                                   \
		asm volatile("msr " #reg ", %0" : : "r"(val)); \
	} while (0)

bool enable_mmu(page_table_t *root)
{
	if (!root) {
		return false;
	}

	// Setup MAIR (Memory Attribute Indirection Register)
	mair_reg_t mair;
	mair.value = 0;

	// Attribute 0: Device-nGnRE (standard for MMIO/UART/Peripherals)
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
	WRITE_SYS_REG(mair_el1, mair);

	// Setup TCR (Translation Control Register)
	tcr_reg_t tcr;
	tcr.value = 0;

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
	// IPS: 40-bit PA limits
	tcr.ips = TCR_IPS_40BIT;
	// EPD1: Disable TTBR1 walking paths
	tcr.epd1 = 1;

	WRITE_SYS_REG(tcr_el1, tcr.value);
	asm volatile("isb");

	// Set ttbr0
	phy_addr root_pa = va_to_pa((virt_addr)root);
	WRITE_SYS_REG(ttbr0_el1, root_pa);
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
