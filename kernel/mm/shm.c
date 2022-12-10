// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/shm.h"

#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/task_types.h"

void shm_init(void)
{
    pr_info("Initializing shared memory subsystem...");
}

vmblock_t shm_allocate(process_t *owner, size_t npages, mmap_flags flags)
{
    vmblock_t block = platform_mm_alloc_pages(owner->pagetable, npages, PGALLOC_HINT_MMAP, VM_READ | VM_WRITE | VM_USER);
    process_attach_mmap(owner, block, VMTYPE_SHM, flags);
    return block;
}
