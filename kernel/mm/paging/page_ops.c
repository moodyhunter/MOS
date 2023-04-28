// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/mm/paging/page_ops.h>
#include <mos/panic.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/tasks/task_types.h>

static void walk_pagetable_dump_callback(const pgt_iteration_info_t *iter_info, const vmblock_t *block, ptr_t block_paddr, void *arg)
{
    MOS_UNUSED(iter_info);
    ptr_t *prev_end_vaddr = (ptr_t *) arg;
    if (block->vaddr - *prev_end_vaddr > MOS_PAGE_SIZE)
    {
        pr_info("  VGROUP: " PTR_FMT, block->vaddr);
    }

    pr_info2("    " PTR_RANGE " -> " PTR_RANGE ", %5zd pages, %c%c%c, %c%c, %s", //
             block->vaddr,                                                       //
             (ptr_t) (block->vaddr + block->npages * MOS_PAGE_SIZE),             //
             block_paddr,                                                        //
             (ptr_t) (block_paddr + block->npages * MOS_PAGE_SIZE),              //
             block->npages,                                                      //
             block->flags & VM_READ ? 'r' : '-',                                 //
             block->flags & VM_WRITE ? 'w' : '-',                                //
             block->flags & VM_EXEC ? 'x' : '-',                                 //
             block->flags & VM_CACHE_DISABLED ? 'C' : '-',                       //
             block->flags & VM_GLOBAL ? 'G' : '-',                               //
             block->flags & VM_USER ? "user" : "kernel"                          //
    );

    *prev_end_vaddr = block->vaddr + block->npages * MOS_PAGE_SIZE;
}

static void mm_dump_pagetable_panic_handler()
{
    const char *cpu_pagetable_source = current_cpu->pagetable.pgd == platform_info->kernel_pgd.pgd ? "Kernel" : NULL;

    if (current_thread)
    {
        pr_emph("Current task: %s (tid: %ld, pid: %ld)", current_process->name, current_thread->tid, current_process->pid);
        pr_emph("Task Page Table:");
        mm_dump_pagetable(current_process->pagetable);
        if (current_cpu->pagetable.pgd == current_process->pagetable.pgd)
            cpu_pagetable_source = "Current Process";
    }
    else
    {
        pr_emph("Kernel Page Table:");
        mm_dump_pagetable(platform_info->kernel_pgd);
    }

    if (cpu_pagetable_source)
    {
        pr_emph("CPU Page Table: %s", cpu_pagetable_source);
    }
    else
    {
        pr_emph("CPU Page Table:");
        mm_dump_pagetable(current_cpu->pagetable);
    }
}

void mm_paging_ops_init()
{
#if MOS_DEBUG_FEATURE(vmm)
    declare_panic_hook(mm_dump_pagetable_panic_handler);
    install_panic_hook(&mm_dump_pagetable_panic_handler_holder);
#endif
}

void mm_dump_pagetable(paging_handle_t handle)
{
    pr_info("Page Table:");
    ptr_t tmp = 0;
    platform_mm_iterate_table(handle, 0, MOS_MAX_VADDR / MOS_PAGE_SIZE, walk_pagetable_dump_callback, &tmp);
}
