// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/tasks/fpu_context.h"

#include "mos/mm/slab_autoinit.h"
#include "mos/platform/platform.h"
#include "mos/tasks/task_types.h"
#include "mos/x86/cpu/cpuid.h"

#include <mos_stdlib.h>

slab_t *fpu_ctx_slab;
SLAB_AUTOINIT("x86_fpu_context", fpu_ctx_slab, fpu_context_t);

void x86_save_fpu_context()
{
    thread_t *thread = current_thread;
    MOS_ASSERT(cpu_has_feature(CPU_FEATURE_FXSR));

    if (thread->platform_options.need_fpu_context)
        return;

    if (unlikely(!thread->platform_options.fpu_state))
        thread->platform_options.fpu_state = kmalloc(fpu_ctx_slab);

    __asm__ volatile("fxsave %0" ::"m"(*thread->platform_options.fpu_state->fpu));
}

void x86_load_fpu_context()
{
    thread_t *thread = current_thread;
    MOS_ASSERT(thread->platform_options.fpu_state);
    __asm__ volatile("fxrstor %0" ::"m"(*thread->platform_options.fpu_state->fpu));
}
