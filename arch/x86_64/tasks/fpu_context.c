// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/tasks/fpu_context.h"

#include "mos/mm/slab.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/setup.h"
#include "mos/tasks/task_types.h"

#include <mos_stdlib.h>

slab_t *xsave_area_slab = NULL;

static void setup_xsave_slab(void)
{
    xsave_area_slab = kmemcache_create("x86.xsave", platform_info->arch_info.xsave_size);
}

MOS_INIT(SLAB_AUTOINIT, setup_xsave_slab);

static const u64 RFBM = ~0ULL;
const reg32_t low = RFBM & 0xFFFFFFFF;
const reg32_t high = RFBM >> 32;

void x86_xsave_thread(thread_t *thread)
{
    if (!thread || thread->mode == THREAD_MODE_KERNEL)
        return; // no, kernel threads don't have these

    pr_dcont(scheduler, "saved.");
    MOS_ASSERT(thread->platform_options.xsaveptr);
    __asm__ volatile("xsave %0" ::"m"(*thread->platform_options.xsaveptr), "a"(low), "d"(high));
}

void x86_xrstor_thread(thread_t *thread)
{
    if (!thread || thread->mode == THREAD_MODE_KERNEL)
        return; // no, kernel threads don't have these

    pr_dcont(scheduler, "restored.");
    MOS_ASSERT(thread->platform_options.xsaveptr);
    __asm__ volatile("xrstor %0" ::"m"(*thread->platform_options.xsaveptr), "a"(low), "d"(high));
}
