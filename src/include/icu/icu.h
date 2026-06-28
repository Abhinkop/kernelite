/**
 * @file icu.h
 * @brief Interrupt Controller Unit (ICU) interface.
 *
 * Provides declarations for early interrupt controller setup and simple
 * interrupt management operations used by the kernel during bootstrap.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-06-07
 */

#ifndef ICU_ICU_H
#define ICU_ICU_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Interrupt handler registration entry.
 *
 * Pairs a handler function with its opaque private data pointer. Stored
 * in the dispatch table indexed by INTID. When an interrupt fires, the
 * dispatcher calls handler(private_data) for the corresponding entry.
 */
typedef struct handler_data_t {
	/** @brief Function to invoke when the corresponding INTID fires.
	 *  NULL indicates no handler is registered for this INTID. */
	void (*handler)(void *private_data);

	/** @brief Opaque pointer passed to the handler on invocation.
	 *  Ownership and lifetime are managed by the registering caller. */
	void *private_data;
} handler_data_t;

/**
 * @brief Initialize the interrupt controller unit.
 *
 * Performs any platform-specific ICU startup required before the kernel can
 * enable and service hardware interrupts.
 *
 * @param fdt Pointer to the Flattened Device Tree (FDT) blob, which may be used
 *            to discover interrupt controller properties and configuration.
 */
void icu_init(const void *fdt);

/**
 * @brief Register a handler for a hardware interrupt source and enable it.
 *
 * @param irq_num        Physical interrupt irq_num to register.
 * @param handler_data Handler function and private data to register.
 * @return true if the handler was registered and the interrupt enabled
 *         successfully, false if the irq_num is out of range or already
 *         registered.
 */
bool icu_register_irq(uint32_t irq_num, handler_data_t handler_data);

/**
 * @brief Unregister a handler for a hardware interrupt source and disable it.
 *
 * Disables the interrupt in the GIC and clears the corresponding entry
 * in the dispatch table. After this call the irq_num will no longer fire
 * and any previously registered handler will not be invoked.
 *
 * @param irq_num Physical interrupt irq_num to unregister.
 */
void icu_unregister_irq(uint32_t irq_num);

/**
 * @brief Top-level IRQ dispatch entry point.
 *
 * Called directly from the EL1 IRQ exception vector. Delegates to the
 * GICv3 handler which acknowledges the interrupt via ICC_IAR1_EL1,
 * dispatches based on INTID, and signals EOI via ICC_EOIR1_EL1.
 *
 * This indirection layer allows the ICU (Interrupt Controller Unit)
 * interface to remain decoupled from the underlying GIC implementation,
 * making it straightforward to swap in a different interrupt controller
 * without modifying the vector table or exception entry code.
 */
void icu_handle_irq(void);

#endif /* ICU_ICU_H */
