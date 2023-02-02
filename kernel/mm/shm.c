// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/shm.h"

#include "mos/mm/paging/paging.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"

void shm_init(void)
{
    pr_info("Initializing shared memory subsystem...");
}

shm_block_t shm_allocate(size_t npages, mmap_flags flags, vm_flags vmflags)
{
    process_t *owner = current_process;
    // TODO: add tracking of shared memory blocks
    mos_debug(shm, "allocating %zu SHM pages in address space " PTR_FMT, npages, owner->pagetable.pgd);
    vmblock_t block = mm_alloc_pages(owner->pagetable, npages, PGALLOC_HINT_MMAP, vmflags);

    process_attach_mmap(owner, block, VMTYPE_SHM, flags);
    return (shm_block_t){ .block = block, .address_space = owner->pagetable };
}

vmblock_t shm_map_shared_block(shm_block_t source)
{
    process_t *owner = current_process;
    mos_debug(shm, "sharing %zu pages from address space " PTR_FMT " to address space " PTR_FMT, source.block.npages, source.address_space.pgd, owner->pagetable.pgd);
    vmblock_t block = mm_get_free_pages(owner->pagetable, source.block.npages, PGALLOC_HINT_MMAP);
    block = mm_copy_maps(source.address_space, source.block.vaddr, owner->pagetable, block.vaddr, source.block.npages);

    process_attach_mmap(owner, block, VMTYPE_SHM, MMAP_PRIVATE);
    return block;
}
