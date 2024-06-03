// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/platform/platform.h>

typedef volatile struct __packed
{
    bool present : 1;
    bool writable : 1;
    bool usermode : 1;
    bool write_through : 1;
    bool cache_disabled : 1;
    bool accessed : 1;
    bool _ignored1 : 1;
    bool page_size : 1; // reserved for pml4e and pml5e, 1 GiB page for pml3e
    u8 available_2 : 3;
    bool hlat_restart : 1; // for HLAT: if 1, linear-address translation is restarted with ordinary paging
    pfn_t page_table_paddr : 40;
    u32 _ignored2 : 11;
    bool no_execute : 1;
} x86_pde64_t, x86_pmde64_t, x86_pude64_t; // PD, PDPT (PMD), PML4 (PUD)

MOS_STATIC_ASSERT(sizeof(x86_pde64_t) == sizeof(pte_content_t), "x86_pde64_t differs from pde_content_t");

typedef volatile struct __packed
{
    bool present : 1;
    bool writable : 1;
    bool usermode : 1;
    bool write_through : 1;
    bool cache_disabled : 1;
    bool accessed : 1;
    bool dirty : 1;
    bool page_size : 1; // must be 1
    bool global : 1;
    u8 available : 2;
    bool hlat_restart : 1; // for HLAT: if 1, linear-address translation is restarted with ordinary paging
    bool pat : 1;
    pfn_t pfn : 39;
    u32 reserved_2 : 7; // must be 0
    u32 protection_key : 4;
    bool no_execute : 1;
} x86_pde64_huge_t, x86_pmde64_huge_t; // 2MiB, 1GiB huge pages for PD, PDPT (PMD) respectively

MOS_STATIC_ASSERT(sizeof(x86_pde64_huge_t) == sizeof(pte_content_t), "x86_pde64_huge_t differs from pde_content_t");

typedef volatile struct __packed
{
    bool present : 1;
    bool writable : 1;
    bool usermode : 1;
    bool write_through : 1;
    bool cache_disabled : 1;
    bool accessed : 1;
    bool dirty : 1;
    bool pat : 1;
    bool global : 1;
    u32 _ignored1 : 2;
    bool hlat_restart : 1;
    pfn_t pfn : 40;
    u32 _ignored2 : 7;
    u32 protection_key : 4;
    bool no_execute : 1;
} x86_pte64_t; // PTE (4 KiB page)

MOS_STATIC_ASSERT(sizeof(x86_pte64_t) == sizeof(pte_content_t), "x86_pde64_t differs from pte_content_t");

void x86_paging_setup(void);
