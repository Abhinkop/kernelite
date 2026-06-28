/**
 * @file icu.c
 * @brief Implementation of the Interrupt Controller Unit (ICU) helpers.
 *
 * Reads the interrupt controller's compatible string from the FDT and
 * dispatches initialization and IRQ (un)registration to the matching
 * platform driver. Currently only "arm,gic-v3" is supported.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-06-07
 */

#include "icu/icu.h"

#include "fdt/fdt.h"
#include "linker/symbols.h"
#include "page_table/page_table.h"
#include "utils/kprintf.h"
#include "utils/string.h"

#include "../drivers/gicv3.h"
#include <stdint.h>

void icu_init(const void *fdt)
{
	if (!fdt || !check_fdt(fdt)) {
		kprintf("Invalid FDT pointer\n");
		return;
	}

	const int intc_node = get_intc_node_offset(fdt);
	if (fdt_is_error(intc_node)) {
		kprintf("No interrupt controller node found in FDT\n");
		return;
	}

	const char *compatible = get_compatible_string(fdt, intc_node);
	if (!compatible) {
		kprintf("Failed to retrieve compatible string for interrupt controller\n");
		return;
	}
	kprintf("Initializing ICU for compatible: %s\n", compatible);
	if (strcmp(compatible, "arm,gic-v3") == 0) {
		kprintf("Detected GICv3 compatible. Initializing GICv3...\n");
		gicv3_init(fdt);
	} else {
		kprintf("Unsupported interrupt controller compatible: %s\n",
			compatible);
	}
}

bool icu_register_irq(uint32_t irq_num, handler_data_t handler_data)
{
	return gicv3_register_irq(irq_num, handler_data);
}

void icu_unregister_irq(uint32_t irq_num)
{
	gicv3_unregister_irq(irq_num);
}

void icu_handle_irq(void)
{
	gicv3_handle_irq();
}
