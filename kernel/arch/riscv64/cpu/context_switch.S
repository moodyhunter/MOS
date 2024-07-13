// SPDX-License-Identifier: GPL-3.0-or-later

.altmacro

.macro push, reg
    addi sp, sp, -8
    sd \reg, 0(sp)
.endm

.macro pop, reg
    ld \reg, 0(sp)
    addi sp, sp, 8
.endm

// void riscv64_do_context_switch(ptr_t *old_stack, ptr_t new_stack, switch_func_t switcher, bool *lock);
// void riscv64_do_context_switch(a0 = old_stack_ptr, a1 = new_stack, a2 = switcher_ptr, a3 = lock_ptr)
.global riscv64_do_context_switch
riscv64_do_context_switch:
    push fp
    push s1
    push s2
    push s3
    push s4
    push s5
    push s6
    push s7
    push s8
    push s9
    push s10
    push s11
    csrr s0, sstatus
    push s0 // sstatus
    csrr s0, sepc
    push s0 // sepc
    csrr s0, stval
    push s0 // stval
    csrr s0, scause
    push s0 // scause
    csrr s0, sscratch
    push s0 // sscratch
    csrr s0, stvec
    push s0 // stvec
    // TODO: FP register is not saved

    // Save the old stack pointer
    sd sp, 0(a0)

    // Load the new stack pointer
    mv sp, a1

    // Unlock the lock
    sb zero, 0(a3)

    // Call the switcher
    jr a2

.global riscv64_normal_switch_impl
riscv64_normal_switch_impl:
    pop s0 // stvec
    csrw stvec, s0
    pop s0 // sscratch
    csrw sscratch, s0
    pop s0 // scause
    csrw scause, s0
    pop s0 // stval
    csrw stval, s0
    pop s0 // sepc
    csrw sepc, s0
    pop s0 // sstatus
    csrw sstatus, s0
    pop s11
    pop s10
    pop s9
    pop s8
    pop s7
    pop s6
    pop s5
    pop s4
    pop s3
    pop s2
    pop s1
    pop fp
    ret

