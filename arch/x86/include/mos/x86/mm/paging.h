// SPDX-Licence-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"
#include "mos/types.h"

typedef struct
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
} __packed pgtable_entry;

static_assert(sizeof(pgtable_entry) == 4, "page_table_entry is not 4 bytes");

typedef struct
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
    u32 page_table_addr : 20;
} __packed pgdir_entry;

static_assert(sizeof(pgdir_entry) == 4, "page_directory_entry is not 4 bytes");

void x86_mm_setup_paging();
void x86_mm_enable_paging();

void x86_mm_map_page(uintptr_t vaddr, uintptr_t paddr, paging_entry_flags flags);
void *x86_mm_alloc_page(size_t n);
bool x86_mm_free_page(void *vaddr, size_t n);
