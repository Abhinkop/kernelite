/**
 * @file handler.c
 * @brief Exception and interrupt handler utilities used during early boot.
 *
 * This file contains a simple generic exception handler used during
 * early bring-up to report the current exception level and stack pointer
 * selection, then halt to avoid returning to the faulting context.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-16
 */

#include "utils/kprintf.h"

#include <stdint.h>

extern void exit(uint32_t code);

/* ESR_EL1 Exception Class values for abort types. */
#define ESR_EC_SHIFT 26U
#define ESR_EC_MASK 0x3FU
#define ESR_EC_IABT_LOW 0x20U /* Instruction Abort from lower EL  */
#define ESR_EC_IABT_CUR 0x21U /* Instruction Abort from current EL */
#define ESR_EC_DABT_LOW 0x24U /* Data Abort from lower EL         */
#define ESR_EC_DABT_CUR 0x25U /* Data Abort from current EL       */
#define ESR_ISS_WNR_BIT (1U << 6) /* ISS[6]: Write-not-Read           */

void generic_handler(void)
{
	kprintf("Generic Handler invoked\n");

	uint64_t current_el = 0;
	uint64_t sp_sel = 0;
	uint64_t esr = 0;
	uint64_t far = 0;
	uint64_t elr = 0;

	asm volatile("mrs %0, CurrentEL" : "=r"(current_el));
	asm volatile("mrs %0, SPSel" : "=r"(sp_sel));
	asm volatile("mrs %0, ESR_EL1" : "=r"(esr));
	asm volatile("mrs %0, FAR_EL1" : "=r"(far));
	asm volatile("mrs %0, ELR_EL1" : "=r"(elr));

	uint64_t el_num = (current_el >> 2) & 0x3;
	uint64_t exc_class = (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;

	kprintf("Current EL        = EL%lx\n", el_num);
	kprintf("Used stack        = EL%lx\n", sp_sel == 0 ? 0UL : el_num);
	kprintf("ELR_EL1 (faulting PC)  = 0x%lx\n", elr);
	kprintf("ESR_EL1           = 0x%lx  (EC=0x%lx)\n", esr, exc_class);

	if (exc_class == ESR_EC_DABT_LOW || exc_class == ESR_EC_DABT_CUR) {
		const char *access = (esr & ESR_ISS_WNR_BIT) ? "WRITE" : "READ";
		kprintf("Data Abort        : FAR_EL1 = 0x%lx  [%s fault]\n",
			far, access);
	} else if (exc_class == ESR_EC_IABT_LOW ||
		   exc_class == ESR_EC_IABT_CUR) {
		kprintf("Instruction Abort : FAR_EL1 = 0x%lx\n", far);
	} else {
		kprintf("FAR_EL1           = 0x%lx  (not an abort)\n", far);
	}

	exit(1);
}
