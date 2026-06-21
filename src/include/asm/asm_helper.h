/**
 * @file asm_helper.h
 * @brief Assembly helper macros for system register access.
 *
 * Declares macros that wrap MRS/MSR instructions for reading and writing
 * ARM64 system registers from C.
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
