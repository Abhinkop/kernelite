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
 * @brief Reserve pages occupied by the kernel image.
 *
 * This function calculates the number of pages occupied by the kernel
 * image based on the linker-provided symbols and reserves those pages in
 * the page allocator to prevent them from being allocated for other purposes.
 *
 * @return bool True if reservation was successful, false otherwise.
 */
bool reserve_kernel_img_pages(void);

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
