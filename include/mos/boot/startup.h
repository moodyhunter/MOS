// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"
#include "mos/types.h"

#define __startup      __section(.mos.startup) __attribute__((__cold__))
#define __startup_data __section(.mos.startup.data)

typedef struct
{
    uintptr_t code_start;
    uintptr_t code_end;

    uintptr_t rodata_start;
    uintptr_t rodata_end;

    uintptr_t rw_start;
    uintptr_t rw_end;

    uintptr_t page_size;

    void (*map_pages)(paging_handle_t handle, uintptr_t virt, uintptr_t phys, size_t size, vm_flags flags);
    void (*unmap_pages)(paging_handle_t handle, uintptr_t virt, size_t size);
} startup_ops_t;

__startup_data extern const startup_ops_t mos_startup_info;
__startup void startup_setup_paging(paging_handle_t handle);
