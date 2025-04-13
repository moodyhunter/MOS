// SPDX-License-Identifier: GPL-3.0-or-later

.altmacro
.extern riscv64_trap_handler

#define CTX_SIZE 264 // sizeof(platform_regs_t)

.macro save_gp i, basereg
    sd x\i, (\i - 1) * 8 (\basereg)
.endm

.macro load_gp i, basereg
    ld x\i, (\i - 1) * 8 (\basereg)
.endm

.macro save_all_gps
    .set i, 1
    .rept 31
        save_gp %i, sp
        .set i, i + 1
    .endr
.endm

.macro restore_all_gps
    .set i, 1
    .rept 31
        .if i != 2
            load_gp %i, sp
        .endif
        .set i, i + 1
    .endr
    load_gp 2, sp // restore sp last as it holds the base address
.endm

.global __riscv64_usermode_trap_entry
.align 2 // align to 4 bytes
__riscv64_usermode_trap_entry:
    csrc sstatus, 2             // disable interrupts
    # U-mode -> S-mode trap
    # exchange sp with sscratch
    csrrw sp, sscratch, sp      // sscratch = user sp
    addi sp, sp, -CTX_SIZE      // allocate ALLOC_SIZE bytes on the stack
    save_all_gps

    csrr s1, sscratch           // read user sp
    sd s1, 8(sp)                // store it in the context (instead of kernel sp)

    mv a0, sp
    call invoke_trap_handler
    unimp

.global __riscv64_trap_entry
.align 2 // align to 4 bytes
__riscv64_trap_entry:
    addi sp, sp, -CTX_SIZE      // allocate ALLOC_SIZE bytes on the stack
    save_all_gps

    ld s1, 8(sp)                // fixup the stack pointer
    addi s1, s1, CTX_SIZE
    sd s1, 8(sp)                // store it back

    mv a0, sp
    call invoke_trap_handler
    unimp

invoke_trap_handler:
    // read scause, stval and sepc as arguments
    csrr a1, scause
    csrr a2, stval
    csrr a3, sepc

    // call do_trap
    call riscv64_trap_handler
    // fallthrough

.global riscv64_trap_exit
riscv64_trap_exit:
    mv sp, a0       // arg0 contains the registers pointer
    restore_all_gps
    sret
    unimp
