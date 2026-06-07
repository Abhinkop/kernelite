/**
 * @file asm_helper.h
 * @brief Assembly helper macros and functions.
 * @brief Header that interfaces with libfdt for Device Tree Blob (DTB) parsing.
 *
 * Declares the kernel-facing FDT helper APIs used to validate and extract
 * memory mapping information from the device tree.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-16
 */

#ifndef ASM_ASM_HELPER_H
#define ASM_ASM_HELPER_H

/** Read a system register */
#define READ_SYS_REG(reg, val)                             \
	do {                                               \
		asm volatile("mrs %0, " #reg : "=r"(val)); \
	} while (0)

/** Write to a system register */
#define WRITE_SYS_REG(reg, val)                                \
	do {                                                   \
		asm volatile("msr " #reg ", %0" : : "r"(val)); \
	} while (0)

#endif /* ASM_ASM_HELPER_H */
