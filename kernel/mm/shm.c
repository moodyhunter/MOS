// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/shm.h"

#include "mos/mm/paging/paging.h"
#include "mos/tasks/process.h"

vmblock_t shm_allocate(size_t npages, vmap_fork_mode_t mode, vm_flags vmflags)
{
    process_t *owner = current_process;
    mos_debug(shm, "allocating %zu SHM pages in address space " PTR_FMT, npages, owner->pagetable.pgd);
    vmblock_t block = mm_alloc_pages(owner->pagetable, npages, MOS_ADDR_USER_MMAP, VALLOC_DEFAULT, vmflags);
    if (block.npages == 0)
    {
        pr_warn("failed to allocate shared memory block");
        return (vmblock_t){ 0 };
    }

    process_attach_mmap(owner, block, VMTYPE_MMAP, (vmap_flags_t){ .fork_mode = mode });
    return block;
}

vmblock_t shm_map_shared_block(vmblock_t source, vmap_fork_mode_t mode)
{
    process_t *owner = current_process;
    mos_debug(shm, "sharing %zu pages from address space " PTR_FMT " to address space " PTR_FMT, source.npages, source.address_space.pgd, owner->pagetable.pgd);
    const uintptr_t vaddr = mm_get_free_pages(owner->pagetable, source.npages, MOS_ADDR_USER_MMAP, VALLOC_DEFAULT);
    const vmblock_t block = mm_copy_maps(source.address_space, source.vaddr, owner->pagetable, vaddr, source.npages, MM_COPY_ALLOCATED);
    process_attach_mmap(owner, block, VMTYPE_MMAP, (vmap_flags_t){ .fork_mode = mode });
    return block;
}
