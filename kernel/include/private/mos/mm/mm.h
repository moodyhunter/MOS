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
    VMAP_HEAP,  // heap
    VMAP_STACK, // stack (user)
    VMAP_FILE,  // file mapping
    VMAP_MMAP,  // mmap mapping
} vmap_content_t;

typedef enum
{
    VMAP_TYPE_PRIVATE = MMAP_PRIVATE, // there will be distinct copies of the memory region in the child process
    VMAP_TYPE_SHARED = MMAP_SHARED,   // the memory region will be shared between the parent and child processes
} vmap_type_t;

typedef struct
{
    bool is_present, is_write, is_user, is_exec;
    phyframe_t *faulting_page;      ///< the frame that contains the copy-on-write data (if any)
    const phyframe_t *backing_page; ///< the frame that contains the data for this page, the on_fault handler should set this
} pagefault_t;

typedef enum
{
    VMFAULT_COMPLETE,             ///< no further action is needed, the page is correctly mapped now
    VMFAULT_MAP_BACKING_PAGE_RO,  ///< the caller should map the backing page into the faulting address, and mark it non-writable
    VMFAULT_MAP_BACKING_PAGE,     ///< the caller should map the backing page into the faulting address
    VMFAULT_COPY_BACKING_PAGE,    ///< the caller should copy the backing page into the faulting address
    VMFAULT_CANNOT_HANDLE = 0xff, ///< the handler cannot handle this fault
} vmfault_result_t;

typedef struct _vmap vmap_t;

typedef vmfault_result_t (*vmfault_handler_t)(vmap_t *vmap, ptr_t fault_addr, pagefault_t *info);

typedef struct _vmap
{
    as_linked_list;
    spinlock_t lock;

    ptr_t vaddr; // virtual addresses
    size_t npages;
    vm_flags vmflags; // the expected flags for the region, regardless of the copy-on-write state
    mm_context_t *mmctx;

    io_t *io;        // the io object that (possibly) backs this vmap
    off_t io_offset; // the offset in the io object, page-aligned

    vmap_content_t content;
    vmap_type_t type;
    vmap_stat_t stat;
    vmfault_handler_t on_fault;
} vmap_t;

#define pfn_va(pfn)        ((ptr_t) (platform_info->direct_map_base + (pfn) *MOS_PAGE_SIZE))
#define va_pfn(va)         ((((ptr_t) (va)) - platform_info->direct_map_base) / MOS_PAGE_SIZE)
#define va_phyframe(va)    (&phyframes[va_pfn(va)])
#define phyframe_va(frame) ((ptr_t) pfn_va(phyframe_pfn(frame)))
#define pa_va(pa)          ((ptr_t) (pa) + platform_info->direct_map_base)

phyframe_t *mm_get_free_page(void);
phyframe_t *mm_get_free_page_raw(void);
phyframe_t *mm_get_free_pages(size_t npages);

void mm_free_page(phyframe_t *frame);
void mm_free_pages(phyframe_t *frame, size_t npages);

/**
 * @brief Create a user-mode platform-dependent page table.
 * @return mm_context_t The created page table.
 * @note A platform-independent page-map is also created.
 */
mm_context_t *mm_create_context(void);

/**
 * @brief Destroy a user-mode platform-dependent page table.
 * @param table The page table to destroy.
 * @note The platform-independent page-map is also destroyed.
 */
void mm_destroy_context(mm_context_t *table);

/**
 * @brief Lock and unlock a pair of mm_context_t objects.
 *
 * @param ctx1 The first context
 * @param ctx2 The second context
 */
void mm_lock_ctx_pair(mm_context_t *ctx1, mm_context_t *ctx2);
void mm_unlock_ctx_pair(mm_context_t *ctx1, mm_context_t *ctx2);

mm_context_t *mm_switch_context(mm_context_t *new_ctx);

/**
 * @brief Create a vmap object and insert it into the address space.
 *
 * @param mmctx The address space
 * @param vaddr Starting virtual address of the region
 * @param npages Number of pages in the region
 * @return vmap_t* The created vmap object, with its lock held for further initialization.
 */
vmap_t *vmap_create(mm_context_t *mmctx, ptr_t vaddr, size_t npages);

/**
 * @brief Destroy a vmap object, and unmmap the region.
 *
 * @param vmap The vmap object
 * @note The vmap object will be freed.
 */
void vmap_destroy(vmap_t *vmap);

/**
 * @brief Get the vmap object for a virtual address.
 *
 * @param mmctx The address space to search
 * @param vaddr The virtual address
 * @param out_offset An optional pointer to receive the offset of the address in the vmap
 * @return vmap_t* The vmap object, or NULL if not found, with its lock held.
 */
vmap_t *vmap_obtain(mm_context_t *mmctx, ptr_t vaddr, size_t *out_offset);

/**
 * @brief Finalize the initialization of a vmap object.
 *
 * @param vmap The vmap object
 * @param content The content type of the region, \see vmap_content_t
 * @param type The fork behavior of the region, \see vmap_type_t
 * @note The vmap object must be locked, and will be unlocked after this function returns.
 */
void vmap_finalise_init(vmap_t *vmap, vmap_content_t content, vmap_type_t type);

/**
 * @brief Helper function to resolve a copy-on-write fault.
 *
 * @param vmap The vmap object
 * @param fault_addr The fault address
 * @param info The page fault info
 * @return vmfault_result_t always VMFAULT_COMPLETE
 */
[[nodiscard("resolve completed")]] vmfault_result_t mm_resolve_cow_fault(vmap_t *vmap, ptr_t fault_addr, pagefault_t *info);

/**
 * @brief Handle a page fault
 *
 * @param fault_addr The fault address
 * @param info The page fault info
 * @return true If the fault is handled
 */
bool mm_handle_fault(ptr_t fault_addr, pagefault_t *info);
