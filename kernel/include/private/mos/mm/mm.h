// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/mmstat.h"
#include "mos/mm/physical/pmm.h"
#include "mos/platform/platform.h"

#include <mos/lib/structures/list.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/mm/mm_types.h>

typedef enum
{
    VMAP_UNKNOWN = 0,
    VMAP_CODE,   // code
    VMAP_DATA,   // data
    VMAP_HEAP,   // heap
    VMAP_STACK,  // stack (user)
    VMAP_KSTACK, // stack (kernel)
    VMAP_FILE,   // file mapping
    VMAP_MMAP,   // mmap mapping
} vmap_content_t;

typedef enum
{
    VMAP_FORK_INVALID = 0,            // invalid fork mode, this never happens
    VMAP_FORK_ZEROED = 1,             // only used for kernel stack
    VMAP_FORK_PRIVATE = MMAP_PRIVATE, // there will be distinct copies of the memory region in the child process
    VMAP_FORK_SHARED = MMAP_SHARED,   // the memory region will be shared between the parent and child processes
} vmap_fork_behavior_t;

typedef struct
{
    bool page_present, op_write, userfault;
} pagefault_info_t;

typedef struct _vmap_t
{
    as_linked_list;
    spinlock_t lock;

    ptr_t vaddr; // virtual addresses
    size_t npages;
    vm_flags vmflags; // the expected flags for the region, regardless of the copy-on-write state
    mm_context_t *mmctx;

    vmap_content_t content;
    vmap_fork_behavior_t fork_behavior;
    vmap_mstat_t stat;

    bool (*on_fault)(struct _vmap_t *this_vmap, ptr_t fault_addr, const pagefault_info_t *fault_info);
} vmap_t;

phyframe_t *mm_get_free_page(void);
phyframe_t *mm_get_free_page_raw(void);
phyframe_t *mm_get_free_pages(size_t npages);

void mm_free_page(phyframe_t *frame);
void mm_free_pages(phyframe_t *frame, size_t npages);

/**
 * @brief Create a vmap object and insert it into the address space.
 *
 * @param mmctx
 * @param vaddr
 * @param npages
 * @return vmap_t* The created vmap object, with its lock held for further initialization.
 */
vmap_t *vmap_create(mm_context_t *mmctx, ptr_t vaddr, size_t npages);
vmap_t *vmap_obtain(mm_context_t *mmctx, ptr_t vaddr, size_t *out_offset);
void vmap_finalise_init(vmap_t *vmap, vmap_content_t content, vmap_fork_behavior_t fork_behavior);

/**
 * @brief Handle a page fault
 *
 * @param fault_addr The fault address
 * @param info The page fault info
 * @return true If the fault is handled
 */
bool mm_handle_fault(ptr_t fault_addr, const pagefault_info_t *info);
