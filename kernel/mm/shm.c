// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/shm.h"

#include "mos/mm/paging/paging.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/task_types.h"

void shm_init(void)
{
    pr_info("Initializing shared memory subsystem...");
}

shm_block_t shm_allocate(size_t npages, vmap_fork_mode_t mode, vm_flags vmflags)
{
    process_t *owner = current_process;
    // TODO: add tracking of shared memory blocks
    mos_debug(shm, "allocating %zu SHM pages in address space " PTR_FMT, npages, owner->pagetable.pgd);
    vmblock_t block = mm_alloc_pages(owner->pagetable, npages, PGALLOC_HINT_MMAP, vmflags);
    if (block.npages == 0)
    {
        pr_warn("failed to allocate shared memory block");
        return (shm_block_t){ 0 };
    }

    process_attach_mmap(owner, block, VMTYPE_MMAP, (vmap_flags_t){ .fork_mode = mode });
    return (shm_block_t){ .block = block, .address_space = owner->pagetable };
}

vmblock_t shm_map_shared_block(shm_block_t source, vmap_fork_mode_t mode)
{
    if (source.block.npages == 0 || source.address_space.pgd == 0)
    {
        pr_warn("attempted to map invalid shared memory block");
        return (vmblock_t){ 0 };
    }

    process_t *owner = current_process;
    mos_debug(shm, "sharing %zu pages from address space " PTR_FMT " to address space " PTR_FMT, source.block.npages, source.address_space.pgd, owner->pagetable.pgd);
    const uintptr_t vaddr_base = mm_get_free_pages(owner->pagetable, source.block.npages, PGALLOC_HINT_MMAP);

    const vmblock_t block = mm_copy_mapping(source.address_space, source.block.vaddr, owner->pagetable, vaddr_base, source.block.npages, MM_COPY_DEFAULT);
    process_attach_mmap(owner, block, VMTYPE_MMAP, (vmap_flags_t){ .fork_mode = mode });
    return block;
}
