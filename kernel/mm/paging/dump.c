// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/dump.h"

#include "mos/mm/paging/iterator.h"
#include "mos/printk.h"
#include "mos/tasks/task_types.h"

static void pagetable_do_dump(ptr_t vaddr, ptr_t vaddr_end, vm_flags flags, pfn_t pfn, pfn_t pfn_end, ptr_t *prev_end_vaddr)
{
    if (vaddr - *prev_end_vaddr > MOS_PAGE_SIZE)
        pr_info("  VGROUP: " PTR_FMT, vaddr);

    pr_info2("    " PTR_RANGE " -> " PFN_RANGE ", %5zd pages, %pvf, %c%c, %s", //
             vaddr,                                                            //
             vaddr_end,                                                        //
             pfn,                                                              //
             pfn_end,                                                          //
             (ALIGN_UP_TO_PAGE(vaddr_end) - vaddr) / MOS_PAGE_SIZE,            //
             (void *) &flags,                                                  //
             flags & VM_CACHE_DISABLED ? 'C' : '-',                            //
             flags & VM_GLOBAL ? 'G' : '-',                                    //
             flags & VM_USER ? "user" : "kernel"                               //
    );

    *prev_end_vaddr = vaddr_end;
}

void mm_dump_pagetable(mm_context_t *mmctx)
{
    pr_info("Page Table:");
    ptr_t tmp = 0;
    spinlock_acquire(&mmctx->mm_lock);

    pagetable_iter_t iter = { 0 };
    pagetable_iter_init(&iter, mmctx->pgd, 0, 0x00007fffffffffff);

    pagetable_iter_range_t *range;
    while ((range = pagetable_iter_next(&iter)))
        if (range->present)
            pagetable_do_dump(range->vaddr, range->vaddr_end, range->flags, range->pfn, range->pfn_end, &tmp);

    pagetable_iter_init(&iter, mmctx->pgd, 0xffff800000000000, 0xffffffffffffffff);
    while ((range = pagetable_iter_next(&iter)))
        if (range->present)
            pagetable_do_dump(range->vaddr, range->vaddr_end, range->flags, range->pfn, range->pfn_end, &tmp);

    spinlock_release(&mmctx->mm_lock);
}

void mm_dump_current_pagetable()
{
    const char *cpu_pagetable_source = current_cpu->mm_context == platform_info->kernel_mm ? "Kernel" : NULL;

    if (current_thread)
    {
        pr_emph("Current task: thread %pt, process %pp", (void *) current_thread, (void *) current_process);
        pr_emph("Task Page Table:");
        mm_dump_pagetable(current_process->mm);
        if (current_cpu->mm_context == current_process->mm)
            cpu_pagetable_source = "Current Process";
    }
    else
    {
        pr_emph("Kernel Page Table:");
        mm_dump_pagetable(platform_info->kernel_mm);
    }

    if (cpu_pagetable_source)
    {
        pr_emph("CPU Page Table: %s", cpu_pagetable_source);
    }
    else
    {
        pr_emph("CPU Page Table:");
        mm_dump_pagetable(current_cpu->mm_context);
    }
}
