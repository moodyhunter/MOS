// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/tasks/fpu_context.hpp"

#include "mos/platform/platform.hpp"
#include "mos/syslog/printk.hpp"
#include "mos/tasks/task_types.hpp"

#include <mos/allocator.hpp>
#include <mos_stdlib.hpp>

static const u64 RFBM = ~0ULL;
const reg32_t low = RFBM & 0xFFFFFFFF;
const reg32_t high = RFBM >> 32;

void x86_xsave_thread(Thread *thread)
{
    if (!thread || thread->mode == THREAD_MODE_KERNEL)
        return; // no, kernel threads don't have these

    if (!thread->platform_options.xsaveptr)
        return; // this happens when the thread is being execve'd

    pr_dcont(scheduler, "saved.");
    __asm__ volatile("xsave %0" ::"m"(*thread->platform_options.xsaveptr), "a"(low), "d"(high));
}

void x86_xrstor_thread(Thread *thread)
{
    if (!thread || thread->mode == THREAD_MODE_KERNEL)
        return; // no, kernel threads don't have these

    if (!thread->platform_options.xsaveptr)
        return; // this happens when the thread is being execve'd

    pr_dcont(scheduler, "restored.");
    __asm__ volatile("xrstor %0" ::"m"(*thread->platform_options.xsaveptr), "a"(low), "d"(high));
}
