// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/tasks/fpu_context.h"

#include "mos/mm/slab_autoinit.h"
#include "mos/platform/platform.h"
#include "mos/tasks/task_types.h"
#include "mos/x86/cpu/cpuid.h"

#include <mos_stdlib.h>

void x86_save_fpu_context()
{
    thread_t *thread = current_thread;
    MOS_ASSERT(cpu_has_feature(CPU_FEATURE_FXSR));

    if (thread->platform_options.need_fpu_context)
        return;

    if (unlikely(!thread->platform_options.xsaveptr))
        thread->platform_options.xsaveptr = kmalloc(platform_info->arch_info.xsave_size);

    __asm__ volatile("xsave %0" ::"m"(*thread->platform_options.xsaveptr));
}

void x86_load_fpu_context()
{
    thread_t *thread = current_thread;
    MOS_ASSERT(thread->platform_options.xsaveptr);

    if (thread->platform_options.need_fpu_context)
        return;

    __asm__ volatile("xrstor %0" ::"m"(*thread->platform_options.xsaveptr));
}
