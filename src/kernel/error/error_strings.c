/**
 * @file error_strings.c
 * @brief Implementation of error code to string translation.
 *
 * Provides the run-time mapping from numeric kernel error codes to human-
 * readable messages used by diagnostic subsystems.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-04-11
 */

#include "error/error_strings.h"

const char *error_to_string(long code)
{
	switch (code) {
#ifndef __DOXYGEN__
/** @cond INTERNAL */
/**
 * @brief Internal X-Macro expansion to generate switch cases.
 *
 * The STRING_MAP macro is defined in error_codes.h and contains lines of the
 * form: X(ERROR_CODE_NAME, "Error message string") This will expand to: case
 * ERROR_CODE_NAME: return "Error message string";
 *
 * @param val The error code token to translate (e.g., ERROR_CODE_NAME).
 * @param str The corresponding string literal error message.
 */
#define X(val, str) \
	case val:   \
		return str;

		/* Expand the map defined in the header */
		STRING_MAP

#undef X
/** @endcond */
#endif /* __DOXYGEN__ */
	default:
		return "UNKNOWN";
	}
}
