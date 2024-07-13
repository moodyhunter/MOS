// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/interrupt/interrupt.h"
#include "mos/ksyscall_entry.h"
#include "mos/mm/paging/table_ops.h"
#include "mos/platform/platform.h"
#include "mos/riscv64/cpu/cpu.h"
#include "mos/riscv64/cpu/plic.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/signal.h"

void riscv64_trap_handler(platform_regs_t *regs, reg_t scause, reg_t stval, reg_t sepc)
{
    write_csr(stvec, __riscv64_trap_entry);
    current_cpu->interrupt_regs = regs;
    regs->sepc = sepc;
    regs->sstatus = read_csr(sstatus);
    const bool is_userspace = (regs->sstatus & SSTATUS_SPP) == 0;
    const bool is_interrupt = scause & BIT(63);
    const long exception = scause & ~BIT(63);

    const char *name = "<unknown>";
    if (is_interrupt)
    {
        switch (exception)
        {
            case 1: name = "Supervisor software interrupt"; break;
            case 5: name = "Supervisor timer interrupt"; goto handle_timer;
            case 9: name = "Supervisor external interrupt"; goto handle_irq;
            case 13: name = "Counter-overflow interrupt"; break;
            default: name = exception >= 16 ? "Designated for platform use" : "<reserved>";
        }
    }
    else
    {
        switch (exception)
        {
            case 0: name = "Instruction address misaligned"; break;
            case 1: name = "Instruction access fault"; break;
            case 2: name = "Illegal instruction"; goto handle_ii;
            case 3: name = "Breakpoint"; goto handle_bp;
            case 4: name = "Load address misaligned"; break;
            case 5: name = "Load access fault"; break;
            case 6: name = "Store/AMO address misaligned"; break;
            case 7: name = "Store/AMO access fault"; break;
            case 8: name = "Environment call from U-mode"; goto handle_syscall;
            case 9: name = "Environment call from S-mode"; break;
            case 12: name = "Instruction page fault"; goto handle_pf;
            case 13: name = "Load page fault"; goto handle_pf;
            case 15: name = "Store/AMO page fault"; goto handle_pf;
            case 18: name = "Software check"; break;
            case 19: name = "Hardware error"; break;
            default: name = ((exception >= 24 && exception <= 31) || (exception >= 48 && exception <= 63)) ? "Designated for custom use" : "<reserved>";
        }
    }

    pr_info("riscv64 trap: sepc=" PTR_FMT ", scause" PTR_FMT ", stval=" PTR_FMT ", %s, code = %ld, interrupt = %d", sepc, scause, stval, name, exception, is_interrupt);
    MOS_ASSERT(!"Unhandled exception");

handle_irq:
    const u32 irq = plic_claim_irq();
    interrupt_entry(irq);
    plic_complete(irq);
    goto leave;

handle_bp:
    signal_send_to_thread(current_thread, SIGTRAP);
    goto leave;

handle_timer:
    const reg_t stime = read_csr(time);
    write_csr(stimecmp, stime + 500 * 1000); // 0.5 ms
    spinlock_acquire(&current_thread->state_lock);
    reschedule();
    goto leave;

handle_syscall:
    regs->sepc += 4; // increase sepc to the next instruction
    const long ret = ksyscall_enter(regs->a7, regs->a0, regs->a1, regs->a2, regs->a3, regs->a4, regs->a5);
    goto leave;

handle_ii:
    if (sepc > MOS_KERNEL_START_VADDR)
    {
        try_handle_kernel_panics(sepc);
        MOS_ASSERT(!"Kernel mode illegal instruction");
    }

    signal_send_to_process(current_process, SIGILL);
    goto leave;

handle_pf:
    MOS_ASSERT_X(!is_interrupt, "Page faults should not be interrupts");

    const bool present = mm_do_get_present(current_cpu->mm_context->pgd, stval);

    pagefault_t pfinfo = {
        .ip = sepc,
        .is_exec = exception == 12,
        .is_write = exception == 15,
        .is_present = present,
        .is_user = is_userspace,
        .regs = regs,
    };
    mm_handle_fault(stval, &pfinfo);
    goto leave;

leave:
    if (is_userspace)
    {
        if (!is_interrupt && exception == 8)
            signal_exit_to_user_prepare_syscall(regs, regs->a7, ret);
        else
            signal_exit_to_user_prepare(regs);
        platform_return_to_userspace(regs);
    }

    MOS_ASSERT_X(!is_userspace, "should not return to userspace here");
    riscv64_trap_exit(regs);
}
