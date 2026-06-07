/**
 * @file mem_layout.c
 * @brief Implementation of memory layout utilities.
 *
 * Provides functions for managing the kernel's memory layout.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-25
 */

#include "mem_layout/mem_layout.h"

/** @brief Base virtual address for the kernel.
 * initially set to 0 until we map the kernel to high memory.
 */
static virt_addr kernel_base_va = 0;

void update_kernel_base_va(void)
{
	kernel_base_va = kernel_base;
}

phy_addr va_to_pa(virt_addr v_addr)
{
	return (phy_addr)v_addr - kernel_base_va;
}

virt_addr pa_to_va(phy_addr p_addr)
{
	return kernel_base_va + p_addr;
}
