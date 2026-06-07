/**
 * @file trampoline.s
 * @brief Trampoline code for switching to high virtual addresses.
 *
 * This assembly code is responsible for switching the execution context to use high virtual addresses
 * after the MMU has been enabled. It adjusts the return address and stack pointer to point to their
 * corresponding high virtual addresses, and then returns to the caller, which will now execute in the
 * high VA space.
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-25
 */

.global switch_to_high_va;           
.type switch_to_high_va, % function; 
switch_to_high_va:
    movz    x0, #0xFFFF, lsl #48        // x0 = 0xFFFF000000000000
    add     lr, lr, x0                  // LR -> high VA of return address
    add     sp, sp, x0                  // SP -> high VA of stack
    isb
    ret                                 // jumps to high VA via updated LR
.size switch_to_high_va, .- switch_to_high_va
