// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/tasks/context.hpp"

#include "mos/platform/platform_defs.hpp"
#include "mos/x86/descriptors/descriptors.hpp"
#include "mos/x86/tasks/fpu_context.hpp"

#include <mos/lib/structures/stack.hpp>
#include <mos/mos_global.h>
#include <mos/platform/platform.hpp>
#include <mos/syslog/printk.hpp>
#include <mos/tasks/schedule.hpp>
#include <mos/tasks/task_types.hpp>
#include <mos/types.hpp>
#include <mos/x86/cpu/cpu.hpp>
#include <mos/x86/mm/paging_impl.hpp>
#include <mos/x86/x86_interrupt.hpp>
#include <mos/x86/x86_platform.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

typedef void (*switch_func_t)();

extern "C" void x86_normal_switch_impl();
extern "C" void x86_context_switch_impl(ptr_t *old_stack, ptr_t new_kstack, switch_func_t switcher, bool *lock);
mos::Slab<u8> xsave_area_slab("x86.xsave", 0);

static void x86_start_kernel_thread()
{
    platform_regs_t *regs = platform_thread_regs(current_thread);
    const thread_entry_t entry = (thread_entry_t) regs->ip;
    void *const arg = (void *) regs->di;
    entry(arg);
    MOS_UNREACHABLE();
}

static void x86_start_user_thread()
{
    x86_interrupt_return_impl(platform_thread_regs(current_thread));
}

static platform_regs_t *x86_setup_thread_common(Thread *thread)
{
    MOS_ASSERT_X(thread->platform_options.xsaveptr == NULL, "xsaveptr should be NULL");
    thread->platform_options.xsaveptr = xsave_area_slab.create();
    thread->k_stack.head -= sizeof(platform_regs_t);
    platform_regs_t *regs = platform_thread_regs(thread);
    *regs = (platform_regs_t) {};

    regs->cs = thread->mode == THREAD_MODE_KERNEL ? GDT_SEGMENT_KCODE : GDT_SEGMENT_USERCODE | 3;
    regs->ss = thread->mode == THREAD_MODE_KERNEL ? GDT_SEGMENT_KDATA : GDT_SEGMENT_USERDATA | 3;
    regs->sp = thread->mode == THREAD_MODE_KERNEL ? thread->k_stack.top : thread->u_stack.top;

    if (thread->mode == THREAD_MODE_USER)
    {
        regs->eflags = 0x202;
        if (thread->owner->platform_options.iopl)
            regs->eflags |= 0x3000;
    }

    return regs;
}

void platform_context_setup_main_thread(Thread *thread, ptr_t entry, ptr_t sp, int argc, ptr_t argv, ptr_t envp)
{
    platform_regs_t *regs = x86_setup_thread_common(thread);
    regs->ip = entry;
    regs->di = argc;
    regs->si = argv;
    regs->dx = envp;
    regs->sp = sp;
}

void platform_context_cleanup(Thread *thread)
{
    if (thread->mode == THREAD_MODE_USER)
        if (thread->platform_options.xsaveptr)
            kfree(thread->platform_options.xsaveptr), thread->platform_options.xsaveptr = NULL;
}

void platform_context_setup_child_thread(Thread *thread, thread_entry_t entry, void *arg)
{
    platform_regs_t *regs = x86_setup_thread_common(thread);
    regs->di = (ptr_t) arg;
    regs->ip = (ptr_t) entry;

    if (thread->mode == THREAD_MODE_KERNEL)
        return;

    MOS_ASSERT(thread->owner->mm == current_mm);
    MOS_ASSERT(thread != thread->owner->main_thread);

    regs->di = (ptr_t) arg;          // argument
    regs->sp = thread->u_stack.head; // update the stack pointer
}

void platform_context_clone(Thread *from, Thread *to)
{
    platform_regs_t *to_regs = platform_thread_regs(to);
    *to_regs = *platform_thread_regs(from);
    to_regs->ax = 0; // return 0 for the child

    // synchronise the sp of user stack
    if (to->mode == THREAD_MODE_USER)
    {
        to->u_stack.head = to_regs->sp;
        to->platform_options.xsaveptr = xsave_area_slab.create();
        memcpy(to->platform_options.xsaveptr, from->platform_options.xsaveptr, xsave_area_slab.size());
    }

    to->platform_options.fs_base = from->platform_options.fs_base;
    to->platform_options.gs_base = from->platform_options.gs_base;
    to->k_stack.head -= sizeof(platform_regs_t);
}

void platform_switch_to_thread(Thread *current, Thread *new_thread, ContextSwitchBehaviorFlags switch_flags)
{
    const switch_func_t switch_func = [=]() -> switch_func_t
    {
        switch (switch_flags)
        {
            case SWITCH_TO_NEW_USER_THREAD: return x86_start_user_thread; break;
            case SWITCH_TO_NEW_KERNEL_THREAD: return x86_start_kernel_thread; break;
            default: return x86_normal_switch_impl; break;
        }
    }();

    if (current)
        x86_xsave_thread(current);

    x86_xrstor_thread(new_thread);
    x86_set_fsbase(new_thread);

    current_cpu->thread = new_thread;
    __atomic_store_n(&per_cpu(x86_cpu_descriptor)->tss.rspN[0], new_thread->k_stack.top, __ATOMIC_SEQ_CST);

    ptr_t trash = 0;
    ptr_t *const stack_ptr = current ? &current->k_stack.head : &trash;

    bool trash_lock = false;
    bool *const lock = current ? &current->state_lock.flag : &trash_lock;
    x86_context_switch_impl(stack_ptr, new_thread->k_stack.head, switch_func, lock);
}

void x86_set_fsbase(Thread *thread)
{
    __asm__ volatile("wrfsbase %0" ::"r"(thread->platform_options.fs_base) : "memory");
}
