// SPDX-License-Identifier: GPL-3.0-or-later

.extern riscv64_trap_handler

.global riscv64_trap_entry
.align 2 // align to 4 bytes
riscv64_trap_entry:
    addi sp, sp, -256 // allocate 256 bytes on the stack
    sd ra, 0(sp)
    sd sp, 8(sp)
    sd gp, 16(sp)
    sd tp, 24(sp)
    sd t0, 32(sp)
    sd t1, 40(sp)
    sd t2, 48(sp)
    sd fp, 56(sp)
    sd s1, 64(sp)
    sd a0, 72(sp)
    sd a1, 80(sp)
    sd a2, 88(sp)
    sd a3, 96(sp)
    sd a4, 104(sp)
    sd a5, 112(sp)
    sd a6, 120(sp)
    sd a7, 128(sp)
    sd s2, 136(sp)
    sd s3, 144(sp)
    sd s4, 152(sp)
    sd s5, 160(sp)
    sd s6, 168(sp)
    sd s7, 176(sp)
    sd s8, 184(sp)
    sd s9, 192(sp)
    sd s10, 200(sp)
    sd s11, 208(sp)
    sd t3, 216(sp)
    sd t4, 224(sp)
    sd t5, 232(sp)
    sd t6, 240(sp)

    // now we have saved all registers, time to fixup the stack pointer
    ld s1, 8(sp) // load the saved stack pointer
    addi s1, s1, 256 // fixup the stack pointer
    sd s1, 8(sp) // save the fixed stack pointer

    // read scause, stval and sepc as arguments
    mv a0, sp
    csrr a1, scause
    csrr a2, stval
    csrr a3, sepc

    // call do_trap
    call riscv64_trap_handler
    unimp


.global riscv64_trap_exit
riscv64_trap_exit:
    mv sp, a0 // arg0 contains the registers pointer
    ld t6, 240(sp)
    ld t5, 232(sp)
    ld t4, 224(sp)
    ld t3, 216(sp)
    ld s11, 208(sp)
    ld s10, 200(sp)
    ld s9, 192(sp)
    ld s8, 184(sp)
    ld s7, 176(sp)
    ld s6, 168(sp)
    ld s5, 160(sp)
    ld s4, 152(sp)
    ld s3, 144(sp)
    ld s2, 136(sp)
    ld a7, 128(sp)
    ld a6, 120(sp)
    ld a5, 112(sp)
    ld a4, 104(sp)
    ld a3, 96(sp)
    ld a2, 88(sp)
    ld a1, 80(sp)
    ld a0, 72(sp)
    ld s1, 64(sp)
    ld fp, 56(sp)
    ld t2, 48(sp)
    ld t1, 40(sp)
    ld t0, 32(sp)
    ld tp, 24(sp)
    ld gp, 16(sp)
    ld sp, 8(sp)
    ld ra, 0(sp)
    sret

