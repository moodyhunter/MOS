// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/bitmap.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/types.h"
#include "mos/x86/x86_platform.h"

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

    u32 phys_addr : 20;
} __packed x86_pgtable_entry;

static_assert(sizeof(x86_pgtable_entry) == 4, "page_table_entry is not 4 bytes");

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

static_assert(sizeof(x86_pgdir_entry) == 4, "page_directory_entry is not 4 bytes");

#define X86_MM_PAGEMAP_NLINES ALIGN_UP(X86_MAX_MEM_SIZE / MOS_PAGE_SIZE / BITMAP_LINE_BITS, BITMAP_LINE_BITS)

// !! FIXME: This is HUGE for a process, consider allocate it on demand
// !! FIXME: This is HUGE for a process, consider allocate it on demand
// !! FIXME: This is HUGE for a process, consider allocate it on demand
typedef struct x86_pg_infra_t
{
    x86_pgdir_entry pgdir[1024];
    x86_pgtable_entry pgtable[1024 * 1024];
    bitmap_line_t page_map[X86_MM_PAGEMAP_NLINES];
} x86_pg_infra_t;

always_inline x86_pg_infra_t *x86_get_pg_infra(paging_handle_t table)
{
    return (x86_pg_infra_t *) table.ptr;
}

vmblock_t pg_page_alloc(x86_pg_infra_t *pg, size_t n, pgalloc_hints flags, vm_flags vm_flags);
vmblock_t pg_page_alloc_at(x86_pg_infra_t *pg, uintptr_t vaddr, size_t n_page, vm_flags vm_flags);
vmblock_t pg_page_get_free(x86_pg_infra_t *pgt, size_t n, pgalloc_hints flags);

void pg_page_free(x86_pg_infra_t *pg, uintptr_t vaddr, size_t n);

void pg_page_flag(x86_pg_infra_t *pg, uintptr_t vaddr, size_t n, vm_flags flags);

uintptr_t pg_page_get_mapped_paddr(x86_pg_infra_t *pg, uintptr_t vaddr);
vm_flags pg_page_get_flags(x86_pg_infra_t *pg, uintptr_t vaddr);
bool pg_page_get_is_mapped(x86_pg_infra_t *pg, uintptr_t vaddr);
void pg_map_pages(x86_pg_infra_t *pg, uintptr_t vaddr_start, uintptr_t paddr_start, size_t n_page, vm_flags flags);
void pg_unmap_pages(x86_pg_infra_t *pg, uintptr_t vaddr_start, size_t n_page);

void pg_do_map_page(x86_pg_infra_t *pg, uintptr_t vaddr, uintptr_t paddr, vm_flags flags);
void pg_do_map_pages(x86_pg_infra_t *pg, uintptr_t vaddr_start, uintptr_t paddr_start, size_t n_page, vm_flags flags);
void pg_do_unmap_page(x86_pg_infra_t *pg, uintptr_t vaddr);
void pg_do_unmap_pages(x86_pg_infra_t *pg, uintptr_t vaddr_start, size_t n_page);
