/**
 * @file gicv3.h
 * @brief ARM GICv3 interrupt controller definitions.
 *
 * Declares the public GICv3 initialization and management API used by the
 * kernel's interrupt controller unit (ICU) layer.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-06-14
 */

#ifndef DRIVERS_GICV3_H
#define DRIVERS_GICV3_H

#include "icu/icu.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize the ARM GICv3 interrupt controller.
 *
 * @param fdt  Pointer to the Flattened Device Tree blob used to discover the
 *             interrupt controller configuration.
 */
void gicv3_init(const void *fdt);

/**
 * @brief Register a handler for a hardware interrupt source and enable it.
 *
 * Installs the given handler and private data into the dispatch table for
 * the specified INTID, then configures and enables the interrupt in the
 * GIC. The handler will be invoked with private_data on every subsequent
 * assertion of this interrupt.
 *
 * @param irq_num      Physical interrupt INTID to register (0-1019).
 * @param handler_data Handler function and private data to register.
 * @return true if the handler was registered and the interrupt enabled
 *         successfully, false if the INTID is out of range or GIC
 *         configuration failed.
 */
bool gicv3_register_irq(uint32_t irq_num, handler_data_t handler_data);

/**
 * @brief Unregister a handler for a hardware interrupt source and disable it.
 *
 * Disables the interrupt in the GIC and clears the corresponding entry
 * in the dispatch table. After this call the INTID will no longer fire
 * and any previously registered handler will not be invoked.
 *
 * @param irq_num Physical interrupt INTID to unregister (0-1019).
 */
void gicv3_unregister_irq(uint32_t irq_num);

/**
 * @brief EL1 IRQ handler.
 *
 * Acknowledges the interrupt by reading ICC_IAR1_EL1, dispatches based
 * on INTID, then signals EOI via ICC_EOIR1_EL1.
 *
 * Must be called with IRQs already masked (hardware does this automatically
 * on exception entry). IRQs are re-enabled on exception return (ERET).
 */
void gicv3_handle_irq(void);

#endif /* DRIVERS_GICV3_H */
