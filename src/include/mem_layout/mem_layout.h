/**
 * @file mem_layout.h
 * @brief Memory layout definitions.
 *
 * Defines the memory layout for the kernel.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-25
 */

#ifndef MEM_LAYOUT_H
#define MEM_LAYOUT_H

#include <stdint.h>

/** @brief Type for representing physical addresses. */
typedef uint64_t phy_addr;

/** @brief Type for representing virtual addresses. */
typedef uint64_t virt_addr;

/** @brief Memory layout base addresses. */
enum mem_layout_bases {
	/** Start of user space (0x0) */
	user_space = 0x0000000000000000UL,

	/** Base of kernel space */
	kernel_base = 0xFFFF000000000000UL,
};

/** @brief Update the kernel base virtual address. */
void update_kernel_base_va(void);

/**
 * @brief Convert a virtual address to a physical address.
 * @param v_addr The virtual address to convert.
 * @return The corresponding physical address.
 */
phy_addr va_to_pa(virt_addr v_addr);

/**
 * @brief Convert a physical address to a virtual address.
 * @param p_addr The physical address to convert.
 * @return The corresponding virtual address.
 */
virt_addr pa_to_va(phy_addr p_addr);

#endif /* MEM_LAYOUT_H */
