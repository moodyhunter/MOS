// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/pml_types.h"

#include <mos/lib/sync/spinlock.h>

typedef struct _page_map page_map_t;

typedef struct
{
    pmlmax_t pagetable; // supersedes pgd

    spinlock_t *lock;
    page_map_t *um_page_map;
} mm_context_t;
