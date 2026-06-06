/**
 * @file error_strings.h
 * @brief Human-readable string mapping for kernel error codes.
 *
 * Provides lookup support for converting numeric error codes into readable
 * diagnostics used by panic and logging routines.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-04-11
 */

#ifndef ERROR_STRINGS_H
#define ERROR_STRINGS_H

#include "error_codes.h"

/**
 * @def STRING_MAP
 * @brief master mapping table for error codes and strings.
 *
 * This macro uses the X-Macro pattern: X(constant, string).
 * To add a new error message, define the constant in error_codes.h
 * and add a corresponding entry here.
 */
#define STRING_MAP                       \
	X(ERROR_CODE_SUCCESS, "SUCCESS") \
	X(ERROR_CODE_NOT_EL1, "CPU not in EL1")

/**
 * @brief Converts a numeric error code into a human-readable string.
 *
 * This function maps internal kernel error constants (e.g.,
 * ERROR_CODE_NOT_EL1) to descriptive string literals. It is primarily used by
 * the panic handler to provide diagnostic feedback over UART when a fatal error
 * occurs.
 *
 * @param code The numeric error code triggered by the kernel or bootloader.
 * @return A pointer to a constant null-terminated string representing the
 * error. Returns "UNKNOWN" if the code is not recognized.
 *
 * @note This function is designed to be safe for use during early boot or
 * panic states; it does not allocate memory and relies solely on
 * strings stored in the .rodata section.
 */
const char *error_to_string(long code);

#endif /* ERROR_STRINGS_H */
