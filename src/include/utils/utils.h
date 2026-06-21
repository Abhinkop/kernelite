/**
 * @file utils.h
 * @brief Utility functions for kernel development.
 *
 * This module contains utility functions for kernel development, including
 * functions for reserving kernel image pages, setting up the global page
 * allocator, and other helper functions used across the kernel.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-25
 */

#ifndef UTILS_UTILS_H
#define UTILS_UTILS_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief AArch64 CurrentEL register.
 *
 * Contains the current Exception Level. Read-only.
 * The EL field is at bits [3:2]; bits [1:0] are RES0.
 */
typedef union current_el_t {
	/** @brief Raw 64-bit register value. */
	uint64_t raw;
	struct __attribute__((packed)) {
		/** @brief [1:0] Reserved. */
		uint32_t res0 : 2;

		/** @brief [3:2] EL: Current Exception Level.
		 *  0b00 = EL0, 0b01 = EL1, 0b10 = EL2, 0b11 = EL3. */
		uint32_t el : 2;

		/** @brief [63:4] Reserved. */
		uint64_t res0_rest : 60;
	};
} current_el_t;
_Static_assert(sizeof(current_el_t) == 8, "CurrentEL must be 64 bits");

/**
 * @brief AArch64 Multiprocessor Affinity Register (MPIDR_EL1, read-only).
 *
 * Identifies the PE within a multiprocessor system. In a simple single-cluster
 * system (e.g. QEMU virt), Aff1/Aff2/Aff3 are 0 and Aff0 is the core number.
 *
 * Compare AffinityValue (Aff3.Aff2.Aff1.Aff0) against GICR_TYPER.AffinityValue
 * to find the Redistributor frame belonging to this PE.
 *
 * Note:
 * - This is a system register, not a memory-mapped register.
 * - Read via `mrs <Xn>, mpidr_el1`.
 * - Bits [63:40] are RES1 or implementation-defined; mask them before use.
 */
typedef union mpidr_el1_t {
	/** @brief Raw 64-bit register value. */
	uint64_t raw;
	struct __attribute__((packed)) {
		/** @brief [7:0]  Aff0: Thread/core ID within a cluster. On most
		 *  simple systems this is the linear core number. When MT==1,
		 * this identifies the thread within a core instead. */
		uint32_t aff0 : 8;

		/** @brief [15:8] Aff1: Cluster ID. Groups of cores sharing L2
		 * cache or a cluster block typically share the same Aff1. */
		uint32_t aff1 : 8;

		/** @brief [23:16] Aff2: Higher-level cluster or socket ID. */
		uint32_t aff2 : 8;

		/** @brief [24] MT: Multithreading. When 1, Aff0 identifies a
		 * thread within a core rather than a core within a cluster. */
		bool mt : 1;

		/** @brief [29:25] Reserved. */
		uint32_t res0_29_25 : 5;

		/** @brief [30] U: Uniprocessor flag. Set to 1 if the
		 * implementation is a single PE with no affinity structure. */
		bool uniprocessor : 1;

		/** @brief [31] RES1: Always reads as 1 at EL1. */
		uint32_t res1 : 1;

		/** @brief [39:32] Aff3: Highest-level affinity (multi-socket or
		 *  NUMA node). Zero on most embedded/server implementations. */
		uint32_t aff3 : 8;

		/** @brief [63:40] RES1 / implementation defined. Mask before
		 * use. */
		uint32_t res0_63_40 : 24;
	};
} mpidr_el1_t;
_Static_assert(sizeof(mpidr_el1_t) == 8, "MPIDR_EL1 must be 64 bits");

/**
 * @brief Gets the core id.
 * @return the core id.
 */
uint32_t get_core_id(void);

/**
 * @brief Prints the current exeception level.
 */
void print_current_el(void);

/**
 * @brief Set up the global page allocator.
 *
 * This function initializes the global page allocator based on the memory map
 * obtained from the Device Tree Blob (FDT).
 *
 * @param fdt_addr Pointer to the FDT blob.
 * @return bool True if initialization was successful, false otherwise.
 */
bool setup_page_allocator(const void *fdt_addr);

#endif /* UTILS_UTILS_H */
