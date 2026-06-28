/**
 * @file main.c
 * @brief Entry point for the ARM System Register Decoder tool.
 * @details Provides a CLI and interactive interface to decode various ARM
 * system registers using pre-defined bitfield structures.
 * * @author Abhin Parekadan Jose
 * @date 2026-04-12
 */

#include "sctlr_el1_32.h"
#include "sctlr_el1_64.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/**
 * @struct reg_entry_t
 * @brief Map register string names to their respective decoder functions.
 */
typedef struct {
	/** @brief Human-readable register name (e.g., "SCTLR_EL1_64") */
	const char *name;

	/** @brief Function pointer to the specific decoder implementation */
	void (*decode)(uint64_t);
} reg_entry_t;

/**
 * @brief Table of supported registers for decoding.
 * @details Add new register definitions here to extend the tool's capabilities.
 */
static const reg_entry_t register_table[] = {
	{ "SCTLR_EL1_32", decode_sctlr_el1_32 },
	{ "SCTLR_EL1_64", decode_sctlr_el1_64 },
};

/** @brief Number of registers currently supported in the table. */
#define REG_COUNT (sizeof(register_table) / sizeof(reg_entry_t))

/**
 * @brief Prints all supported register names to the console.
 */
void list_registers()
{
	printf("\nSupported Registers:\n");
	for (unsigned int i = 0; i < REG_COUNT; i++) {
		printf("  - %s\n", register_table[i].name);
	}
}

/**
 * @brief Searches for a register by name and executes its decoder.
 * * @param name The string name of the register to find (case-insensitive).
 * @param val The raw 64-bit value to be decoded.
 * @return int 0 on success, -1 if the register name was not found.
 */
int decode_by_name(const char *name, uint64_t val)
{
	for (unsigned int i = 0; i < REG_COUNT; i++) {
		if (strcasecmp(name, register_table[i].name) == 0) {
			printf("\nDecoding %s...\n", register_table[i].name);
			register_table[i].decode(val);
			return 0;
		}
	}
	return -1;
}

/**
 * @brief Main execution logic.
 * @details Handles command-line arguments if provided (argc >= 3), otherwise
 * falls back to an interactive user prompt.
 * * @param argc Argument count.
 * @param argv Argument vector.
 * @return int 0 on success, 1 on error/invalid input.
 */
int main(int argc, char *argv[])
{
	uint64_t val = 0;
	char reg_name[64];

	// CASE 1: Command Line Arguments Provided
	if (argc >= 3) {
		strncpy(reg_name, argv[1], sizeof(reg_name));
		val = strtoull(argv[2], NULL, 16);

		if (decode_by_name(reg_name, val) != 0) {
			printf("Error: Register '%s' not supported.\n",
			       reg_name);
			list_registers();
			return 1;
		}
		return 0;
	}

	// CASE 2: Interactive Mode
	printf("--- ARM System Register Decoder ---\n");
	printf("Usage: %s <reg_name> <hex_val>\n", argv[0]);

	list_registers();

	printf("\nEnter register name: ");
	if (scanf("%63s", reg_name) != 1)
		return 1;

	printf("Enter value in hex: ");
	if (scanf("%lx", &val) != 1)
		return 1;

	if (decode_by_name(reg_name, val) != 0) {
		printf("Invalid register name.\n");
		return 1;
	}

	return 0;
}
