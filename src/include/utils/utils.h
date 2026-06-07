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

/**
 * @brief Set up the global page allocator.
 *
 * This function initializes the global page allocator based on the memory map
 * obtained from the Device Tree Blob (FDT).
 *
 * @param fdt_addr Pointer to the FDT address.
 * @return bool True if initialization was successful, false otherwise.
 */
bool setup_page_allocator(const void *fdt_addr);

#endif /* UTILS_UTILS_H */
