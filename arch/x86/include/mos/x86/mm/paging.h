// SPDX-Licence-Identifier: GPL-3.0-or-later

#pragma once

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

    u32 mem_addr : 20;
} __packed page_table_entry;

static_assert(sizeof(page_table_entry) == 4, "page_table_entry is not 4 bytes");

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
    u32 table_address : 20;
} __packed page_directory_entry;

static_assert(sizeof(page_directory_entry) == 4, "page_directory_entry is not 4 bytes");

void x86_enable_paging();
void *x86_alloc_page(size_t n);
bool x86_free_page(void *ptr, size_t n);
