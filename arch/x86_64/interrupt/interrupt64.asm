; SPDX-License-Identifier: GPL-3.0-or-later

[bits 64]

%define IRQ_BASE          0x20 ; as in x86.h
%define ISR_MAX_COUNT     255
%define IRQ_MAX_COUNT     16

%define REGSIZE           8

extern x86_interrupt_entry

global irq_stub_table
global isr_stub_table

; ! When the CPU calls the interrupt handlers, the CPU pushes these values onto the stack in this order:
; ! SS -> rsp -> EFLAGS -> CS -> EIP
; ! If the gate type is not a trap gate, the CPU will clear the interrupt flag.

; handler for a ISR with its error code (already pushed onto the stack)
%macro ISR_handler_ec 1
isr_stub_%+%1:
    cli
    nop                             ; ! If the interrupt is an exception, the CPU will push an error code onto the stack, as a QWORD.
    push    %1                      ; interrupt number
    jmp     do_handle_interrupt
%endmacro

; handler for a ISR
%macro ISR_handler 1
isr_stub_%+%1:
    cli
    push    0                       ; error code (not used)
    push    %1                      ; interrupt number
    jmp     do_handle_interrupt
%endmacro

; handler for an IRQ
%macro IRQ_handler 1
irq_stub_%1:
    cli
    push    0                       ; error code (not used)
    push    %1 + IRQ_BASE           ; IRQ number
    jmp     do_handle_interrupt
%endmacro

ISR_handler     0 ; Divide-by-zero Error
ISR_handler     1 ; Debug
ISR_handler     2 ; NMI (Non-maskable Interrupt)
ISR_handler     3 ; Breakpoint
ISR_handler     4 ; Overflow
ISR_handler     5 ; Bounds Range Exceeded
ISR_handler     6 ; Invalid Opcode
ISR_handler     7 ; Device Not Available
ISR_handler_ec  8 ; Double Fault
ISR_handler     9 ; Coprocessor Segment Overrun (Since the 486 this is handled by a GPF instead like it already did with non-FPU memory accesses)
ISR_handler_ec 10 ; Invalid TSS
ISR_handler_ec 11 ; Segment Not Present
ISR_handler_ec 12 ; Stack-Segment Fault
ISR_handler_ec 13 ; General Protection Fault
ISR_handler_ec 14 ; Page Fault
ISR_handler    15 ; Reserved
ISR_handler    16 ; x87 FPU Floating-Point Error
ISR_handler_ec 17 ; Alignment Check
ISR_handler    18 ; Machine Check
ISR_handler    19 ; SIMD Floating-Point Exception
ISR_handler    20 ; Virtualization Exception
ISR_handler    21 ; Control Protection Exception
ISR_handler    22 ; Reserved
ISR_handler    23 ; Reserved
ISR_handler    24 ; Reserved
ISR_handler    25 ; Reserved
ISR_handler    26 ; Reserved
ISR_handler    27 ; Reserved
ISR_handler    28 ; Hypervisor Injection Exception
ISR_handler    29 ; VMM Communication Exception
ISR_handler_ec 30 ; Security Exception
ISR_handler    31 ; Reserved

; It's not really necessary to setup all these ISRs, but
; doing so is easier
other_intrs:
    %assign i 32
    %rep ISR_MAX_COUNT
    ISR_handler i
    %assign i i+1
    %endrep

IRQ_handler     0 ; Programmable Interrupt Timer Interrupt
IRQ_handler     1 ; Keyboard Interrupt
IRQ_handler     2 ; Cascade (used internally by the two PICs. never raised)
IRQ_handler     3 ; COM2
IRQ_handler     4 ; COM1
IRQ_handler     5 ; LPT2
IRQ_handler     6 ; Floppy Disk
IRQ_handler     7 ; LPT1 (probably spurious)
IRQ_handler     8 ; CMOS RTC
IRQ_handler     9 ; Free for peripherals / legacy SCSI / NIC
IRQ_handler    10 ; Free for peripherals / SCSI / NIC
IRQ_handler    11 ; Free for peripherals / SCSI / NIC
IRQ_handler    12 ; PS2 Mouse
IRQ_handler    13 ; FPU / Coprocessor / Inter-processor
IRQ_handler    14 ; Primary ATA Hard Disk
IRQ_handler    15 ; Secondary ATA Hard Disk

isr_stub_table:
    %assign i 0
    %rep ISR_MAX_COUNT
        dq isr_stub_%+i ; use DQ instead if targeting 64-bit
    %assign i i+1
    %endrep

irq_stub_table:
    %assign i 0
    %rep IRQ_MAX_COUNT
        dq irq_stub_%+i
    %assign i i+1
    %endrep

do_handle_interrupt:
    push    rax
    push    rbx
    push    rcx
    push    rdx

    push    rbp
    push    rsi
    push    rdi

    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15

    cld                             ; clears the DF flag in the RFLAGS register.
                                    ; so that string operations increment the index registers (RSI and/or RDI).

    mov     rdi, rsp
    call    x86_interrupt_entry    ; x86_interrupt_entry(ptr_t sp)
    ud2                             ; if x86_interrupt_entry returns, it's a bug
.end:

global x86_interrupt_return_impl:function (x86_interrupt_return_impl.end - x86_interrupt_return_impl)
x86_interrupt_return_impl:
    mov     rsp, rdi

    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8

    pop     rdi
    pop     rsi
    pop     rbp

    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax

    add     rsp, 2 * REGSIZE
    iretq
.end:
