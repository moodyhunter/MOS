// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/clocksource.hpp"
#include "mos/interrupt/interrupt.hpp"
#include "mos/ksyscall_entry.hpp"
#include "mos/mm/paging/table_ops.hpp"
#include "mos/platform/platform.hpp"
#include "mos/riscv64/cpu/cpu.hpp"
#include "mos/riscv64/cpu/plic.hpp"
#include "mos/tasks/schedule.hpp"
#include "mos/tasks/signal.hpp"

extern clocksource_t goldfish;

extern "C" platform_regs_t *riscv64_trap_handler(platform_regs_t *regs, reg_t scause, reg_t stval, reg_t sepc)
{
    write_csr(stvec, __riscv64_trap_entry);
    current_cpu->interrupt_regs = regs;
    regs->sepc = sepc;
    regs->sstatus = read_csr(sstatus);
    const bool is_userspace = (regs->sstatus & SSTATUS_SPP) == 0;
    const bool is_interrupt = scause & BIT(63);
    const long exception = scause & ~BIT(63);
    long ret = 0;

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
{
    const u32 irq = plic_claim_irq();
    interrupt_entry(irq);
    plic_complete(irq);
    goto leave;
}
handle_bp:
{
    signal_send_to_thread(current_thread, SIGTRAP);
    goto leave;
}

handle_timer:
{
    const reg_t stime = read_csr(time);
    write_csr(stimecmp, stime + 1000 * 10); // 10ms
    spinlock_acquire(&current_thread->state_lock);
    clocksource_tick(&goldfish);
    reschedule();
    goto leave;
}

handle_syscall:
{
    regs->sepc += 4; // increase sepc to the next instruction
    ret = ksyscall_enter(regs->a7, regs->a0, regs->a1, regs->a2, regs->a3, regs->a4, regs->a5);
    goto leave;
}

handle_ii:
{
    if (sepc > MOS_KERNEL_START_VADDR)
    {
        try_handle_kernel_panics(sepc);
        MOS_ASSERT(!"Kernel mode illegal instruction");
    }

    signal_send_to_process(current_process, SIGILL);
    goto leave;
}

handle_pf:
{
    MOS_ASSERT_X(!is_interrupt, "Page faults should not be interrupts");

    const bool present = mm_do_get_present(current_cpu->mm_context->pgd, stval);

    pagefault_t pfinfo = {
        .is_present = present,
        .is_write = exception == 15,
        .is_user = is_userspace,
        .is_exec = exception == 12,
        .ip = sepc,
        .regs = regs,
    };
    mm_handle_fault(stval, &pfinfo);
    goto leave;
}

leave:
    if (is_userspace)
    {
        ptr<platform_regs_t> syscall_ret_regs;
        if (!is_interrupt && exception == 8)
            syscall_ret_regs = signal_exit_to_user_prepare(regs, regs->a7, ret);
        else
            syscall_ret_regs = signal_exit_to_user_prepare(regs);

        if (syscall_ret_regs)
            *regs = *syscall_ret_regs;

        reg_t sstatus = read_csr(sstatus);
        sstatus &= ~SSTATUS_SPP;
        sstatus |= SSTATUS_SPIE;
        sstatus |= SSTATUS_SUM;

        write_csr(sstatus, sstatus);
        write_csr(sscratch, current_thread->k_stack.top);
        write_csr(stvec, (ptr_t) __riscv64_usermode_trap_entry);
        write_csr(sepc, regs->sepc);
    }

    return regs;
}
