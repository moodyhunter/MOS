// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/slab.h"

#include <mos/mm/cow.h>
#include <mos/mm/paging/dump.h>
#include <mos/mm/physical/pmm.h>
#include <mos/panic.h>
#include <mos/tasks/task_types.h>

void mos_kernel_mm_init(void)
{
    pr_info("initializing kernel memory management");

    mm_cow_init();
    slab_init();

#if MOS_DEBUG_FEATURE(vmm)
    declare_panic_hook(mm_dump_current_pagetable, "Dump page table");
    install_panic_hook(&mm_dump_current_pagetable_holder);
    mm_dump_current_pagetable();
#endif

#if MOS_DEBUG_FEATURE(pmm)
    declare_panic_hook(pmm_dump_lists, "Dump PMM lists");
    install_panic_hook(&pmm_dump_lists_holder);
    pmm_dump_lists();
#endif
}
