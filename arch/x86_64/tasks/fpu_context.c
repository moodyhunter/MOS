// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/tasks/fpu_context.h"

#include "mos/mm/slab.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/platform/platform.h"
#include "mos/tasks/task_types.h"
#include "mos/x86/cpu/cpuid.h"

#include <mos_stdlib.h>

slab_t *xsave_area_slab = NULL;

static void setup_xsave_slab(void)
{
    MOS_ASSERT(cpu_has_feature(CPU_FEATURE_FXSR));
    xsave_area_slab = kmemcache_create("x86.xsave", platform_info->arch_info.xsave_size);
}

MOS_INIT(SLAB_AUTOINIT, setup_xsave_slab);

static const u64 RFBM = ~0ULL;
const reg32_t low = RFBM & 0xFFFFFFFF;
const reg32_t high = RFBM >> 32;

void x86_xsave_current()
{
    thread_t *const thread = current_thread;
    if (thread->mode == THREAD_MODE_KERNEL)
        return; // no, kernel threads don't have these

    MOS_ASSERT(thread->platform_options.xsaveptr);
    __asm__ volatile("xsave %0" ::"m"(*thread->platform_options.xsaveptr), "a"(low), "d"(high));
}

void x86_xrstor_current()
{
    thread_t *const thread = current_thread;
    if (thread->mode == THREAD_MODE_KERNEL)
        return; // no, kernel threads don't have these

    MOS_ASSERT(thread->platform_options.xsaveptr);
    __asm__ volatile("xrstor %0" ::"m"(*thread->platform_options.xsaveptr), "a"(low), "d"(high));
}
