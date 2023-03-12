// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/list.h"
#include "mos/types.h"

/**
 * @defgroup Physical Memory Manager
 * @ingroup mm
 * @brief The physical memory manager (pmm) is responsible for managing physical memory.
 * @{
 */

typedef enum
{
    // 0 is explicitly reserved
    PMM_REGION_USEABLE = 1,
    PMM_REGION_RESERVED = 2,
} pmm_region_type_t;

typedef struct _pmblock pmblock_t;

/**
 * @brief A block of physical memory.
 *
 * @details
 * In the physical memory manager (pmm), there are two types of blocks:
 * - allocated: a block of physical memory that has been allocated by the kernel, and possibly mapped
 *              into many virtual address spaces.
 * - free:      a block of physical memory that is either 'reserved' (e.g. by the bootloader) or
 *              not yet allocated.
 *
 * There is only one linked list in PMM that tracks only the 'free' blocks. For the 'allocated' blocks,
 * MOS uses a reference count to track how many 'users' there are of the block. When the reference count
 * reaches zero, the block is freed, and added to the free list.
 *
 * @note 1. The `allocated' and `free' members are mutually exclusive, and only one of them should be used at a time.
 *
 * @note 2. When allocating multiple pages, the physical memory manager may allocate a non-contiguous list of blocks,
 *          in which case, the linked list in each block will be a list of blocks in the allocated region.
 *
 */
typedef struct _pmblock
{
    uintptr_t paddr;
    size_t npages;

    as_linked_list;
    union
    {
        struct
        {
            atomic_t refcount;
        } allocated;

        struct
        {
            pmm_region_type_t type;
        } free;
    };
} pmblock_t;

/**
 * @brief Initialize a static block of physical memory.
 *
 * @param _name Name of the block
 * @param _paddr Physical address of the block
 * @param _npages Number of pages in the block
 *
 * @note This macro is used to initialize a static block of physical memory.
 *       A static block of physical memory is used for special cases, such as
 *       hardware memory-mapped I/O regions.
 *
 * @note This macro initializes the block's reference count to 1, so that it
 *       will never be freed, so it's 'static'.
 */
#define STATIC_PMBLOCK(_name, _paddr, _npages)                                                                                                                           \
    static pmblock_t _name = {                                                                                                                                           \
        .list_node = LIST_NODE_INIT(_name),                                                                                                                              \
        .paddr = (_paddr),                                                                                                                                               \
        .npages = (_npages),                                                                                                                                             \
        .allocated = { .refcount = 1 },                                                                                                                                  \
    }

/**
 * @brief Read-only list of physical memory regions provided by the bootloader.
 */
extern const list_head *const pmm_free_regions;

/**
 * @brief Dump the physical memory manager's state, (i.e. the free list)
 */
void pmm_dump(void);

/**
 * @brief Add a region of physical memory to the physical memory manager.
 *
 * @param start_addr Starting address of the region
 * @param nbytes Size of the region in bytes
 * @param type Type of the region
 *
 * @note The second parameter is in bytes, not pages, page-aligning will be done internally.
 *
 */
void mos_pmm_add_region(uintptr_t start_addr, size_t nbytes, pmm_region_type_t type);

/**
 * @brief Switch to the kernel heap.
 *
 * @note This function should be called after the kernel heap has been setup, and should only be called once.
 */
void pmm_switch_to_kheap(void);

/**
 * @brief Allocate a block of physical memory.
 *
 * @param n_pages Number of pages to allocate
 *
 * @return Pointer to the allocated block of memory, or NULL if there is not enough memory.
 */
pmblock_t *pmm_allocate(size_t n_pages);

/**
 * @brief Get the physical address of a specific page in a block of physical memory.
 *
 * @param head Pointer to the block of physical memory
 * @param page_idx Index of the page in the block
 */
uintptr_t pmm_get_page_paddr(const pmblock_t *blocks, size_t page_idx);

/**
 * @brief Free a list of blocks of physical memory.
 *
 * @param head Pointer to the block of physical memory
 */
void pmm_free(pmblock_t *blocks);

/**
 * @brief Acquire a list of contiguous pages from a specific region.
 *
 * @param start_addr Starting address of the region
 * @param npages Number of pages to acquire
 * @return The list head of the acquired pages.
 *
 * @note If no requested number of pages are available, the kernel panics.
 */
pmblock_t *pmm_acquire_usable_pages_at(uintptr_t start_addr, size_t npages);

/**
 * @brief Acquire a list of RESERVED contiguous pages.
 *
 * @param start_addr Starting address of the region
 * @param npages Number of pages to acquire
 * @return The list head of the acquired pages.
 *
 * @note If no requested number of pages are available, the kernel panics.
 *
 * @note This function is rarely used
 */
pmblock_t *pmm_acquire_reserved_pages_at(uintptr_t start_addr, size_t npages);

/** @} */
