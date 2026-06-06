
/**
 * @file exit.c
 * @brief Provides a mechanism to exit QEMU with a specific code.
 *
 * Uses the semihosting SYS_EXIT call (HLT #0xF000) to terminate the QEMU
 * guest and propagate an exit code back to the host.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-04-25
 */

#include "utils/kprintf.h"

#include <stdint.h>

/**
 * @brief Exit the guest environment with a semihosting code.
 *
 * Uses the ARM semihosting SYS_EXIT operation to terminate execution under
 * QEMU and report the provided status code to the host.
 *
 * @param code Host-visible exit status.
 */
[[noreturn]] void exit(uint32_t code)
{
	kprintf("Exiting QEMU with code: %u\n", code);

	volatile uint64_t block[2];
	block[0] = 0x20026;
	block[1] = code;

	// NOLINTBEGIN(hicpp-no-assembler)
	__asm__ volatile("mov x0, #0x18\n" /* SYS_EXIT */
			 "mov x1, %0\n" /* pointer to block */
			 "hlt #0xF000\n" ::"r"((uint64_t)block)
			 : "x0", "x1", "memory");
	// NOLINTEND(hicpp-no-assembler)
	__builtin_unreachable();
}
