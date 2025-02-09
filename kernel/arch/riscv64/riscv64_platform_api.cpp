// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/assert.hpp"
#include "mos/platform/platform.hpp"
#include "mos/riscv64/cpu/cpu.hpp"
#include "mos/tasks/signal.hpp"
#include "mos/tasks/task_types.hpp"

#include <mos/platform_syscall.hpp>
#include <mos_stdlib.hpp>

// Platform Context Switching APIs
typedef void (*switch_func_t)();
extern "C" void riscv64_do_context_switch(ptr_t *old_stack, ptr_t new_stack, switch_func_t switcher, bool *lock);
extern "C" void riscv64_normal_switch_impl();

static void riscv64_start_user_thread()
{
    platform_regs_t *regs = platform_thread_regs(current_thread);
    signal_exit_to_user_prepare(regs);
    platform_return_to_userspace(regs);
}

static void riscv64_start_kernel_thread()
{
    platform_regs_t *regs = platform_thread_regs(current_thread);
    const thread_entry_t entry = (thread_entry_t) regs->sepc;
    void *const arg = (void *) regs->a0;
    entry(arg);
    MOS_UNREACHABLE();
}

void platform_shutdown()
{
    (*(volatile u32 *) pa_va(0x100000)) = 0x5555;
    while (1)
        platform_cpu_idle();
}

u32 platform_current_cpu_id()
{
    return 0;
}

void platform_cpu_idle()
{
    __asm__ volatile("wfi");
}

void platform_dump_regs(platform_regs_t *regs)
{
    pr_info("General Purpose Registers:");
    pr_info2("   ra/x1: " PTR_FMT "   sp/x2: " PTR_FMT "   gp/x3: " PTR_FMT "  tp/x4: " PTR_FMT, regs->ra, regs->sp, regs->gp, regs->tp);
    pr_info2("   t0/x5: " PTR_FMT "   t1/x6: " PTR_FMT "   t2/x7: " PTR_FMT "  fp/x8: " PTR_FMT, regs->t0, regs->t1, regs->t2, regs->fp);
    pr_info2("   s1/x9: " PTR_FMT "  a0/x10: " PTR_FMT "  a1/x11: " PTR_FMT " a2/x12: " PTR_FMT, regs->s1, regs->a0, regs->a1, regs->a2);
    pr_info2("  a3/x13: " PTR_FMT "  a4/x14: " PTR_FMT "  a5/x15: " PTR_FMT " a6/x16: " PTR_FMT, regs->a3, regs->a4, regs->a5, regs->a6);
    pr_info2("  a7/x17: " PTR_FMT "  s2/x18: " PTR_FMT "  s3/x19: " PTR_FMT " s4/x20: " PTR_FMT, regs->a7, regs->s2, regs->s3, regs->s4);
    pr_info2("  s5/x21: " PTR_FMT "  s6/x22: " PTR_FMT "  s7/x23: " PTR_FMT " s8/x24: " PTR_FMT, regs->s5, regs->s6, regs->s7, regs->s8);
    pr_info2("  s9/x25: " PTR_FMT " s10/x26: " PTR_FMT " s11/x27: " PTR_FMT " t3/x28: " PTR_FMT, regs->s9, regs->s10, regs->s11, regs->t3);
    pr_info2("  t4/x29: " PTR_FMT "  t5/x30: " PTR_FMT "  t6/x31: " PTR_FMT, regs->t4, regs->t5, regs->t6);
}

platform_regs_t *platform_thread_regs(const thread_t *thread)
{
    return (platform_regs_t *) (thread->k_stack.top - sizeof(platform_regs_t));
}

static void thread_setup_common(thread_t *thread)
{
    thread->k_stack.head = thread->k_stack.top - sizeof(platform_regs_t);
}

// Platform Thread / Process APIs
void platform_context_setup_main_thread(thread_t *thread, ptr_t entry, ptr_t sp, int argc, ptr_t argv, ptr_t envp)
{
    thread_setup_common(thread);
    platform_regs_t *regs = platform_thread_regs(thread);
    regs->sepc = entry;
    regs->a0 = argc;
    regs->a1 = argv;
    regs->a2 = envp;
    regs->sp = sp;
}

void platform_context_setup_child_thread(thread_t *thread, thread_entry_t entry, void *arg)
{
    thread_setup_common(thread);
    platform_regs_t *regs = platform_thread_regs(thread);
    regs->a0 = (ptr_t) arg;
    regs->sepc = (ptr_t) entry;

    if (thread->mode == THREAD_MODE_KERNEL)
        return;

    // for user threads, set up the global pointer
    if (thread->owner == current_process)
        regs->gp = platform_thread_regs(current_thread)->gp;

    MOS_ASSERT(thread->owner->mm == current_mm);
    MOS_ASSERT(thread != thread->owner->main_thread);

    regs->sp = thread->u_stack.head; // update the stack pointer
}

void platform_context_clone(const thread_t *from, thread_t *to)
{
    platform_regs_t *to_regs = platform_thread_regs(to);
    platform_regs_t *from_regs = platform_thread_regs(from);
    *to_regs = *from_regs;
    to_regs->a0 = 0; // return 0 for the child
    if (to->mode == THREAD_MODE_USER)
    {
        to->u_stack.head = to_regs->sp;
    }
    to->k_stack.head -= sizeof(platform_regs_t);
}

void platform_context_cleanup(thread_t *thread)
{
    MOS_UNUSED(thread);
    // nothing to cleanup
}

void platform_interrupt_disable()
{
    const reg_t sstatus = read_csr(sstatus);
    write_csr(sstatus, sstatus & ~SSTATUS_SIE);
}

void platform_interrupt_enable()
{
    const reg_t sstatus = read_csr(sstatus);
    write_csr(sstatus, sstatus | SSTATUS_SIE);
}

void platform_switch_mm(const mm_context_t *new_mm)
{
    write_csr(satp, make_satp(SATP_MODE_SV48, 0, pgd_pfn(new_mm->pgd)));
    __asm__ volatile("sfence.vma zero, zero");
}

#define FLEN 64 // D extension

static void do_save_fp_context(thread_t *thread)
{
    if (thread->mode == THREAD_MODE_KERNEL)
        return;

#define f_op(reg, val) __asm__ volatile("fld " #reg ", %0" : : "m"(thread->platform_options.f[val]))
    f_op(f0, 0);
    f_op(f1, 1);
    f_op(f2, 2);
    f_op(f3, 3);
    f_op(f4, 4);
    f_op(f5, 5);
    f_op(f6, 6);
    f_op(f7, 7);
    f_op(f8, 8);
    f_op(f9, 9);
    f_op(f10, 10);
    f_op(f11, 11);
    f_op(f12, 12);
    f_op(f13, 13);
    f_op(f14, 14);
    f_op(f15, 15);
    f_op(f16, 16);
    f_op(f17, 17);
    f_op(f18, 18);
    f_op(f19, 19);
    f_op(f20, 20);
    f_op(f21, 21);
    f_op(f22, 22);
    f_op(f23, 23);
    f_op(f24, 24);
    f_op(f25, 25);
    f_op(f26, 26);
    f_op(f27, 27);
    f_op(f28, 28);
    f_op(f29, 29);
    f_op(f30, 30);
    f_op(f31, 31);
    thread->platform_options.fcsr = read_csr(fcsr);
#undef f_op
}

void do_restore_fp_context(thread_t *thread)
{
    if (thread->mode == THREAD_MODE_KERNEL)
        return;
#define f_op(reg, val) __asm__ volatile("fsd " #reg ", %0" : : "m"(thread->platform_options.f[val]))
    write_csr(fcsr, thread->platform_options.fcsr);
    f_op(f0, 0);
    f_op(f1, 1);
    f_op(f2, 2);
    f_op(f3, 3);
    f_op(f4, 4);
    f_op(f5, 5);
    f_op(f6, 6);
    f_op(f7, 7);
    f_op(f8, 8);
    f_op(f9, 9);
    f_op(f10, 10);
    f_op(f11, 11);
    f_op(f12, 12);
    f_op(f13, 13);
    f_op(f14, 14);
    f_op(f15, 15);
    f_op(f16, 16);
    f_op(f17, 17);
    f_op(f18, 18);
    f_op(f19, 19);
    f_op(f20, 20);
    f_op(f21, 21);
    f_op(f22, 22);
    f_op(f23, 23);
    f_op(f24, 24);
    f_op(f25, 25);
    f_op(f26, 26);
    f_op(f27, 27);
    f_op(f28, 28);
    f_op(f29, 29);
    f_op(f30, 30);
    f_op(f31, 31);
#undef f_op
}

void platform_switch_to_thread(thread_t *current, thread_t *new_thread, switch_flags_t switch_flags)
{
    const switch_func_t switch_func = statement_expr(switch_func_t, {
        switch (switch_flags)
        {
            case SWITCH_TO_NEW_USER_THREAD: retval = riscv64_start_user_thread; break;
            case SWITCH_TO_NEW_KERNEL_THREAD: retval = riscv64_start_kernel_thread; break;
            default: retval = riscv64_normal_switch_impl; break;
        }
    });

    if (current)
        do_save_fp_context(current);
    do_restore_fp_context(new_thread);

    __atomic_store_n(&current_cpu->thread, new_thread, __ATOMIC_SEQ_CST);

    ptr_t trash = 0;
    ptr_t *const stack_ptr = current ? &current->k_stack.head : &trash;
    bool trash_lock = false;
    bool *const lock = current ? &current->state_lock.flag : &trash_lock;
    riscv64_do_context_switch(stack_ptr, new_thread->k_stack.head, switch_func, lock);
}

[[noreturn]] void platform_return_to_userspace(platform_regs_t *regs)
{
    reg_t sstatus = read_csr(sstatus);
    sstatus &= ~SSTATUS_SPP;
    sstatus |= SSTATUS_SPIE;
    sstatus |= SSTATUS_SUM;
    write_csr(sstatus, sstatus);
    write_csr(sscratch, current_thread->k_stack.top);
    write_csr(stvec, (ptr_t) __riscv64_usermode_trap_entry);
    write_csr(sepc, regs->sepc);
    riscv64_trap_exit(regs);
}

u64 platform_arch_syscall(u64 syscall, u64 arg1, u64 arg2, u64 arg3, u64 arg4)
{
    MOS_UNUSED(arg2);
    MOS_UNUSED(arg3);
    MOS_UNUSED(arg4);

    switch (syscall)
    {
        case RISCV64_SYSCALL_SET_TP:
        {
            platform_thread_regs(current_thread)->tp = arg1;
            current_cpu->interrupt_regs->tp = arg1;
            return 0;
        }
    }
    return 0;
}

void platform_ipi_send(u8 target_cpu, ipi_type_t type)
{
    MOS_UNUSED(target_cpu);
    MOS_UNUSED(type);
    // pr_emerg("platform_ipi_send() not implemented");
}

void platform_dump_stack(platform_regs_t *regs)
{
    ptr_t *caller_fp = (ptr_t *) regs->fp;
    pr_info("Stack dump:");
    for (int i = 0; i < 16; i++)
    {
        pr_info("  %p: %ps", (void *) caller_fp, (void *) *(caller_fp - 1));
        caller_fp = (ptr_t *) *(caller_fp - 2);
        if (!caller_fp || !is_aligned(caller_fp, 16) || (ptr_t) caller_fp < MOS_KERNEL_START_VADDR)
            break;
    }
}

void platform_syscall_setup_restart_context(platform_regs_t *regs, reg_t syscall_nr)
{
    regs->a7 = syscall_nr;
    regs->sepc -= 4; // replay the 'ecall' instruction
}

void platform_syscall_store_retval(platform_regs_t *regs, reg_t result)
{
    regs->a0 = result;
}

void platform_jump_to_signal_handler(const platform_regs_t *regs, const sigreturn_data_t *sigreturn_data, const sigaction_t *sa)
{
    current_thread->u_stack.head = regs->sp - 128;

    // backup previous frame
    stack_push_val(&current_thread->u_stack, *regs);
    stack_push_val(&current_thread->u_stack, *sigreturn_data);

    // Set up the new context
    platform_regs_t ret_regs = *regs;
    ret_regs.sepc = (ptr_t) sa->handler;
    ret_regs.ra = (ptr_t) sa->sa_restorer; // the return address
    ret_regs.a0 = sigreturn_data->signal;  // arg1
    ret_regs.sp = current_thread->u_stack.head;
    platform_return_to_userspace(&ret_regs);
}

void platform_restore_from_signal_handler(void *sp)
{
    current_thread->u_stack.head = (ptr_t) sp;
    sigreturn_data_t data;
    platform_regs_t regs;
    stack_pop_val(&current_thread->u_stack, data);
    stack_pop_val(&current_thread->u_stack, regs);

    signal_on_returned(&data);
    platform_return_to_userspace(&regs);
}
