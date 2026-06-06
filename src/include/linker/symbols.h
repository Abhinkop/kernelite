/**
 * @file symbols.h
 * @brief Symbol definitions from the linker script.
 *
 * Declares the linker-provided symbols used by the kernel to locate the image
 * bounds, zero page allocator bitmap, and other layout metadata.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-04-12
 */
#ifndef LINKER_SYMBOLS_H
#define LINKER_SYMBOLS_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Start of the identity page map directory pages.
 */
extern uint8_t idmap_pg_dir_start[];

/**
 * @brief Start of the kernel page map directory pages.
 */
extern uint8_t kernel_pg_dir_root_start[];

/**
 * @brief Start of the page allocator bitmap.
 */
extern uint8_t page_allocator_bit_map_start[];

/** @brief Linker-provided symbol marking the start of the linked image. */
extern const char image_start;

/** @brief Linker-provided symbol marking the end of the linked image. */
extern const char image_end;

/**
 * @brief Return the size of the linked image in bytes.
 *
 * Uses the linker-provided `image_start` and `image_end` symbols injected
 * by the linker script to compute the kernel image size.
 *
 * @return The number of bytes in the linked kernel image.
 */
static inline size_t get_image_size(void)
{
	return (size_t)(&image_end - &image_start);
}

/**
 * @brief Return the root of the id map
 * @return the pointer to the root of the id map.
 */
static inline void *get_id_map_root()
{
	return (void *)&idmap_pg_dir_start;
}

#endif // LINKER_SYMBOLS_H
