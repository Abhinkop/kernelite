/**
 * @file vector.s
 * @brief Exception vector table and assembly helpers for EL1.
 *
 * This file defines the vector table used to handle synchronous and
 * asynchronous exceptions, along with register save/restore macros and
 * the `install_vectors` entry to set `VBAR_EL1` during early boot.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-16
 */

.section .text

// ─────────────────────────────────────────────────────────────────────────────
// SAVE_REGS / RESTORE_REGS
//   Full general-purpose register frame (x0-x30, 248 bytes).
//   x30 (LR) must be saved because 'bl' to C will overwrite it.
// ─────────────────────────────────────────────────────────────────────────────
.macro SAVE_REGS
    sub sp, sp, #256

    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    str x30,      [sp, #240]
.endm

.macro RESTORE_REGS
    ldr x30,      [sp, #240]
    ldp x28, x29, [sp, #224]
    ldp x26, x27, [sp, #208]
    ldp x24, x25, [sp, #192]
    ldp x22, x23, [sp, #176]
    ldp x20, x21, [sp, #160]
    ldp x18, x19, [sp, #144]
    ldp x16, x17, [sp, #128]
    ldp x14, x15, [sp, #112]
    ldp x12, x13, [sp, #96]
    ldp x10, x11, [sp, #80]
    ldp x8,  x9,  [sp, #64]
    ldp x6,  x7,  [sp, #48]
    ldp x4,  x5,  [sp, #32]
    ldp x2,  x3,  [sp, #16]
    ldp x0,  x1,  [sp, #0]

    add sp, sp, #256
    eret
.endm

.global install_vectors
install_vectors:
    adr x0, vector_table
    msr VBAR_EL1, x0
    isb
    ret

.section .text
.align 11                      // REQUIRED: 2KB for VBAR_EL1

.global vector_table
vector_table:

// ======================================================================
// 0x000 - Exception from the current EL while using SP_EL0
// ======================================================================
b curr_el_sp0_sync

.org vector_table + 0x80
b curr_el_sp0_irq

.org vector_table + 0x100
b curr_el_sp0_fiq

.org vector_table + 0x180
b curr_el_sp0_serror

// ======================================================================
// 0x200 - Exception from the current EL while using SP_ELx
// ======================================================================
.org vector_table + 0x200
b curr_el_curr_sp_sync

.org vector_table + 0x280
b curr_el_curr_sp_irq

.org vector_table + 0x300
b curr_el_curr_sp_fiq

.org vector_table + 0x380
b curr_el_curr_sp_serror

// ======================================================================
// 0x400 - Exception from a lower EL and at least one lower EL is AArch64
// ======================================================================
.org vector_table + 0x400
b lower_el_64_sync

.org vector_table + 0x480
b lower_el_64_irq

.org vector_table + 0x500
b lower_el_64_fiq

.org vector_table + 0x580
b lower_el_64_serror

// ======================================================================
// 0x400 - Exception from a lower EL and all lower ELs are AArch32
// ======================================================================
.org vector_table + 0x600
b lower_el_32_sync

.org vector_table + 0x680
b lower_el_32_irq

.org vector_table + 0x700
b lower_el_32_fiq

.org vector_table + 0x780
b lower_el_32_serror

// NOTE: curr_el_curr_sp_irq (EL1 using SP_EL1, the normal running state) is
// the only vector wired to real IRQ dispatch (el1_irq_handler -> ICU). Every
// other vector below — including IRQs taken from SP_EL0 or from a lower
// EL — falls through to generic_handler, which prints state and halts.
// A lower-EL or SP0 interrupt will currently crash the kernel instead of
// being dispatched.
curr_el_sp0_sync:
curr_el_sp0_irq:
curr_el_sp0_fiq:
curr_el_sp0_serror:

curr_el_curr_sp_sync:
curr_el_curr_sp_serror:
curr_el_curr_sp_fiq:

lower_el_64_sync :
lower_el_64_irq :
lower_el_64_fiq :
lower_el_64_serror :

lower_el_32_sync:
lower_el_32_irq:
lower_el_32_fiq:
lower_el_32_serror:
    SAVE_REGS
    bl generic_handler
    RESTORE_REGS

curr_el_curr_sp_irq:
    SAVE_REGS
    bl el1_irq_handler
    RESTORE_REGS
