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

shm_block_t shm_allocate(process_t *owner, size_t npages, mmap_flags flags, vm_flags vmflags)
{
    pr_info2("allocating %zu SHM pages in address space " PTR_FMT, npages, owner->pagetable.ptr);
    vmblock_t block = platform_mm_alloc_pages(owner->pagetable, npages, PGALLOC_HINT_MMAP, vmflags);
    process_attach_mmap(owner, block, VMTYPE_SHM, flags);
    return (shm_block_t){ .block = block, .address_space = owner->pagetable };
}

vmblock_t shm_map_shared_block(shm_block_t source, process_t *owner)
{
    pr_info2("sharing %zu pages from address space " PTR_FMT " to address space " PTR_FMT, source.block.npages, source.address_space.ptr, owner->pagetable.ptr);
    vmblock_t block = platform_mm_get_free_pages(owner->pagetable, source.block.npages, PGALLOC_HINT_MMAP);
    block = platform_mm_copy_maps(source.address_space, source.block.vaddr, owner->pagetable, block.vaddr, source.block.npages);
    process_attach_mmap(owner, block, VMTYPE_SHM, MMAP_PRIVATE);
    return block;
}
