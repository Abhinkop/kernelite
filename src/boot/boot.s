/**
 * @file boot.s
 * @brief Low-level AArch64 bootstrapper and kernel entry point.
 *
 * Handles the initial CPU state, sets up an emergency stack, clears the BSS
 * section, and transitions execution to the C kernel.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-04-11
 */

#include "error/error_codes.h"

/* --- Macro Definitions --- */

/**
 * @brief Begins a function definition with global visibility.
 * @param name Function name
 */
#define SYM_CODE_START(name)    \
	.global name;           \
	.type name, % function; \
name:

/**
 * @brief Ends a function definition and calculates its size.
 * @param name Function name
 */
#define SYM_CODE_END(name) .size name, .- name

/**
 * @brief Store Literal: Stores a register value into a memory symbol.
 * Uses PC-relative adrp/add for position independence.
 */
.macro  str_l, src, sym, tmp
    adrp    \tmp, \sym
    str     \src, [\tmp, :lo12:\sym]
.endm

/**
 * @brief Load Literal: Loads a value from a memory symbol into a register.
 */
.macro  ldr_l, dst, sym, tmp
    adrp    \tmp, \sym
    ldr     \dst, [\tmp, :lo12:\sym]
.endm

/*
* @dst: destination register (64 bit wide)
* @sym: name of the symbol
*/
.macro	adr_l, dst, sym
adrp	\dst, \sym
add	\dst, \dst, :lo12:\sym
.endm

/*
 * read_ctr - read CTR_EL0. If the system has mismatched register fields,
 * provide the system wide safe value from arm64_ftr_reg_ctrel0.sys_val
 */
.macro	read_ctr, reg
mrs	\reg, ctr_el0			// read CTR
nop
.endm

/*
 * dcache_line_size - get the safe D-cache line size across all CPUs
 */
.macro	dcache_line_size, reg, tmp
read_ctr	\tmp
ubfm		\tmp, \tmp, #16, #19	// cache line size encoding
mov		\reg, #4		// bytes per word
lsl		\reg, \reg, \tmp	// actual cache line size
.endm

#define EL1_VALUE 0x4 /* CurrentEL value for EL1: (0b01 << 2) */

/**
 * @brief Switches to emergency stack and calls the C panic handler.
 * @param error_code The numeric code to pass to panic_print_c.
 */
.macro panic_print error_code
    mov     x20, x30                            // Preserve return address in scratch register
    adrp    x1, emergency_stack_top             // Load emergency stack top
    add     sp, x1, :lo12:emergency_stack_top
    stp     x29, x20, [sp, #-16]!               // Create stack frame (16-byte aligned)
    mov     x29, sp
    ldr     x0, =\error_code                    // Load error code into first arg register
    bl      panic_print_c                       // Call C implementation
    ldp     x29, x30, [sp], #16                 // Restore frame and return address
.endm

/**
 * @brief Debug utility to print a register's value via UART.
 * @param reg0 The register to be printed (e.g., x19).
 */
.macro debug_print_reg reg0
    mov     x20, x30
    adrp    x1, emergency_stack_top
    add     sp, x1, :lo12:emergency_stack_top
    stp     x29, x20, [sp, #-16]!
    mov     x29, sp
    mov     x0, \reg0                           // Move target register to x0 for C call
    bl      print_hex
    ldp     x29, x30, [sp], #16
.endm

/**
 * @brief Asserts that two registers are equal; panics otherwise.
 * @param reg1 First register to compare
 * @param reg2 Second register to compare
 * @param error_code Code to print on failure
 */
.macro assert_eq reg1, reg2, error_code
    cmp     \reg1, \reg2
    b.eq    1f                                  // Forward jump to local label 1 if equal
    panic_print \error_code                     // Call panic macro
2:  wfe                                         // Wait for event (Halt)
    b       2b                                  // Infinite loop
1:  nop
.endm

/* --- Data Section --- */

.section .data
.balign 16
/**
 * @brief Emergency stack for early boot/panic scenarios.
 * AArch64 requires 16-byte alignment for the stack pointer.
 */
emergency_stack:
    .space 256
emergency_stack_top:

/* --- Boot Header --- */

/**
 * @section .header.arm64
 * Standard Linux ARM64 Image Header for bootloader compatibility.
 */
.section ".header.arm64", "ax"
.balign 8
    b       primary_entry   // Jump to actual entry point
    .long   0               // Reserved
    .quad   0               // Text offset
    .quad   0               // Image size
    .quad   0xA             // Flags (Little Endian, 4KB Pages)
    .quad   0, 0, 0         // Reserved
    .ascii  "ARM\x64"       // Magic number
    .long   0               // Reserved

/* --- Text Section --- */

.section ".text"

/**
 * @brief Primary kernel entry point.
 * Performs core-gating, basic HW init, BSS clearing, and jumps to main().
 */
SYM_CODE_START(primary_entry)
    // 1. Core Gating: Only Core 0 continues
    mrs     x19, mpidr_el1
    and     x19, x19, #0xFF
    cbnz    x19, halt

    // Save boot arguments (x0 .. x3) for main()
    mov	x21, x0				// x21=FDT

    // 3. Zero out the BSS section
    adr_l   x0, __bss_start
    ldr     x1, =__bss_size
clear_bss:
    cbz     x1, save_boot_args          // If size is 0, skip
    str     xzr, [x0], #8          // Store Zero Register (64-bit) and post-index
    subs    x1, x1, #1
    bne     clear_bss

save_boot_args:
	adr_l	x0, boot_args			// record the contents of
	str     x21,  [x0]			    // Store addess of fdt in boot args
    dmb	sy
    add	x1, x0, #0x8			    // 8 bytes
	bl	dcache_inval_poc

    bl      setup_sctlr_el1

    // Setup exception vectors.
    bl     install_vectors

    // 2. Set up Primary Stack
    adr_l   x0, stack_top
    mov     sp, x0

jump_main:

    adr_l	x0, boot_args		// Pass pointer to boot_args as first argument to main()
    bl      main                   // Enter C environment

halt:
    adr_l   x1, stack_top
    mov     sp, x1
    bl       exit
SYM_CODE_END(primary_entry)

/**
 * @brief Disables the MMU and verifies current Exception Level.
 */
SYM_CODE_START(setup_sctlr_el1)

#define SCTLR_EL1_64_MMU 0
#define SCTLR_EL1_64_ALIGN_CHECK 1
#define SCTLR_EL1_64_D_CACHE 2
#define SCTLR_EL1_64_I_CACHE 12
#define SCTLR_EL1_64_EOE 24
#define SCTLR_EL1_64_EE 25

    mrs     x19, CurrentEL
    ldr     x0, =EL1_VALUE
    assert_eq x19, x0, ERROR_CODE_NOT_EL1

    mrs     x19, sctlr_el1

    ldr     x0, =1 << SCTLR_EL1_64_MMU 
    bic     x19, x19, x0 // MMU diabled (bit 0 = 0)
    ldr     x0, =1 << SCTLR_EL1_64_ALIGN_CHECK
    orr     x19, x19, x0 // Enable alignment check (bit 1 = 1)
    ldr     x0, =1 << SCTLR_EL1_64_D_CACHE
    bic     x19, x19, x0 // D-cache disabled (bit 2 = 0)
    ldr     x0, =1 << SCTLR_EL1_64_I_CACHE
    orr     x19, x19, x0 // I-cache enabled (bit 12 = 1)
    ldr     x0, =1 << SCTLR_EL1_64_EOE
    bic     x19, x19, x0 // Endianness of exceptions = Little Endian (bit 24 = 0)
    ldr     x0, =1 << SCTLR_EL1_64_EE
    bic     x19, x19, x0 // Endianness of instructions = Little Endian (bit 25 = 0)

    msr     sctlr_el1, x19
    isb
    ret
SYM_CODE_END(setup_sctlr_el1)

/*
 *	dcache_inval_poc(start, end)
 *
 * 	Ensure that any D-cache lines for the interval [start, end)
 * 	are invalidated. Any partial lines at the ends of the interval are
 *	also cleaned to PoC to prevent data loss.
 *
 *	- start   - kernel start address of region
 *	- end     - kernel end address of region
 */
SYM_CODE_START(dcache_inval_poc)
	dcache_line_size x2, x3
	sub	x3, x2, #1
	tst	x1, x3				// end cache line aligned?
	bic	x1, x1, x3
	b.eq	1f
	dc	civac, x1			// clean & invalidate D / U line
1:	tst	x0, x3				// start cache line aligned?
	bic	x0, x0, x3
	b.eq	2f
	dc	civac, x0			// clean & invalidate D / U line
	b	3f
2:	dc	ivac, x0			// invalidate D / U line
3:	add	x0, x0, x2
	cmp	x0, x1
	b.lo	2b
	dsb	sy
	ret
SYM_CODE_END(dcache_inval_poc)
