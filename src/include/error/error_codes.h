/**
 * @file error_codes.h
 * @brief Kernel-wide error definitions and diagnostic utilities.
 *
 * Defines kernel error codes used throughout the bootloader and runtime
 * subsystems for consistent failure reporting.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-04-11
 */

#ifndef ERROR_CODES_H
#define ERROR_CODES_H

/** @brief Success error code. */
#define ERROR_CODE_SUCCESS 0x0

/** @brief Error code for when the current privilege level is not EL1. */
#define ERROR_CODE_NOT_EL1 0x1

#endif // ERROR_CODES_H
