// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/platform/platform.h>

typedef volatile struct
{
    bool present : 1;
    bool writable : 1;
    bool usermode : 1;
    bool write_through : 1;
    bool cache_disabled : 1;
    bool accessed : 1;
    bool dirty : 1;
    bool page_size : 1;
    bool global : 1;

    bool kernel_b0 : 1;
    bool kernel_b1 : 1;
    bool kernel_b2 : 1;

    pfn_t pfn : 20;
} __packed x86_pgtable_entry;

MOS_STATIC_ASSERT(sizeof(x86_pgtable_entry) == 4, "page_table_entry is not 4 bytes");

typedef volatile struct
{
    bool present : 1;
    bool writable : 1;
    bool usermode : 1;
    bool write_through : 1;
    bool cache_disabled : 1;
    bool accessed : 1;
    bool available_1 : 1;
    bool page_sized : 1;
    u8 available_2 : 4;
    u32 page_table_paddr : 20;
} __packed x86_pgdir_entry;

MOS_STATIC_ASSERT(sizeof(x86_pgdir_entry) == 4, "page_directory_entry is not 4 bytes");

#define X86_MM_PAGEMAP_NLINES ALIGN_UP(X86_MAX_MEM_SIZE / MOS_PAGE_SIZE / BITMAP_LINE_BITS, BITMAP_LINE_BITS)

// !! FIXME: This is HUGE for a process, consider allocate it on demand
// !! FIXME: This is HUGE for a process, consider allocate it on demand
// !! FIXME: This is HUGE for a process, consider allocate it on demand
typedef struct x86_pg_infra_t
{
    x86_pgdir_entry pgdir[1024];
    x86_pgtable_entry pgtable[1024 * 1024];
} x86_pg_infra_t;

always_inline x86_pg_infra_t *x86_get_pg_infra(mm_context_t *mmctx)
{
    return (x86_pg_infra_t *) mmctx->pgd;
}

void pg_flag_page(x86_pg_infra_t *pg, ptr_t vaddr, size_t n, vm_flags flags);
ptr_t pg_get_mapped_paddr(x86_pg_infra_t *pg, ptr_t vaddr);
vm_flags pg_get_flags(x86_pg_infra_t *pg, ptr_t vaddr);

void pg_map_page(x86_pg_infra_t *pg, ptr_t vaddr, pfn_t pfn, vm_flags flags);
void pg_unmap_page(x86_pg_infra_t *pg, ptr_t vaddr);

void x86_enable_paging_impl(ptr_t page_dir);
