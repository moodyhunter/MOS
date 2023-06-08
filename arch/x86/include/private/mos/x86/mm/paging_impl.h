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
} __packed x86_pte_t;

MOS_STATIC_ASSERT(sizeof(x86_pte_t) == 4, "x86_pte_t is not 4 bytes");
MOS_STATIC_ASSERT(sizeof(x86_pte_t) == sizeof(pte_content_t), "x86_pte_t differs from pte_content_t");

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
    pfn_t page_table_paddr : 20;
} __packed x86_pde_t;

MOS_STATIC_ASSERT(sizeof(x86_pde_t) == 4, "x86_pde_t is not 4 bytes");
MOS_STATIC_ASSERT(sizeof(x86_pde_t) == sizeof(pte_content_t), "x86_pde_t differs from pde_content_t");

// !! FIXME: This is HUGE for a process, consider allocate it on demand
// !! FIXME: This is HUGE for a process, consider allocate it on demand
// !! FIXME: This is HUGE for a process, consider allocate it on demand
typedef struct x86_pg_infra_t
{
    x86_pde_t pgdir[1024];
    x86_pte_t pgtable[1024 * 1024];
} x86_pg_infra_t;

// defined in enable_paging.asm
extern void x86_enable_paging_impl(ptr_t page_dir);

ptr_t pg_get_mapped_paddr(x86_pg_infra_t *pg, ptr_t vaddr);
vm_flags pg_get_flags(x86_pg_infra_t *pg, ptr_t vaddr);
void pg_unmap_page(x86_pg_infra_t *pg, ptr_t vaddr);

void x86_paging_setup(void);
