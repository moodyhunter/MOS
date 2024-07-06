// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/assert.h"
#include "mos/platform/platform.h"
#include "mos/riscv64/cpu/cpu.h"

MOS_STUB_IMPL(platform_regs_t *platform_thread_regs(const thread_t *thread))
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

// Platform Thread / Process APIs
MOS_STUB_IMPL(void platform_context_setup_main_thread(thread_t *thread, ptr_t entry, ptr_t sp, int argc, ptr_t argv, ptr_t envp))
MOS_STUB_IMPL(void platform_context_setup_child_thread(thread_t *thread, thread_entry_t entry, void *arg))
MOS_STUB_IMPL(void platform_context_clone(const thread_t *from, thread_t *to))
MOS_STUB_IMPL(void platform_context_cleanup(thread_t *thread))

// Platform Context Switching APIs

MOS_STUB_IMPL(void platform_switch_to_thread(thread_t *old_thread, thread_t *new_thread, switch_flags_t switch_flags))
MOS_STUB_IMPL(noreturn void platform_return_to_userspace(platform_regs_t *regs))

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
