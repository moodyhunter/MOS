// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform_defs.h"

#include <mos/mos_global.h>

#define MOS_MAX_PAGE_LEVEL 5

#if MOS_PLATFORM_PAGING_LEVELS > 4
#error "more levels are not supported"
#endif

// ! platform-independent pml types

#define define_pmlx(pmln)                                                                                                                                                \
    typedef struct                                                                                                                                                       \
    {                                                                                                                                                                    \
        pte_content_t content;                                                                                                                                           \
    } __packed pmln##e_t;                                                                                                                                                \
    typedef struct                                                                                                                                                       \
    {                                                                                                                                                                    \
        pmln##e_t *table;                                                                                                                                                \
    } pmln##_t

// nah, your platform must have at least 1 level of paging
define_pmlx(pml1);

#if !defined(MOS_PLATFORM_PML1_SHIFT) || !defined(MOS_PLATFORM_PML1_MASK)
#error "MOS_PLATFORM_PML1_SHIFT and MOS_PLATFORM_PML1_MASK must be defined"
#endif

#define pml1_index(vaddr) ((vaddr >> MOS_PLATFORM_PML1_SHIFT) & MOS_PLATFORM_PML1_MASK)

#if MOS_PLATFORM_PAGING_LEVELS >= 2
define_pmlx(pml2);
#define pml2_index(vaddr) ((vaddr >> MOS_PLATFORM_PML2_SHIFT) & MOS_PLATFORM_PML2_MASK)
#if MOS_CONFIG(MOS_PLATFORM_PML2_HUGE_CAPABLE)
#define PML2_HUGE_MASK (MOS_PLATFORM_PML1_MASK << MOS_PLATFORM_PML1_SHIFT)
#endif
#else
new_named_opaque_type(pml1_t, next, pml2_t);
#endif

#if MOS_PLATFORM_PAGING_LEVELS >= 3
define_pmlx(pml3);
#define pml3_index(vaddr) ((vaddr >> MOS_PLATFORM_PML3_SHIFT) & MOS_PLATFORM_PML3_MASK)
#if MOS_CONFIG(MOS_PLATFORM_PML3_HUGE_CAPABLE)
#define PML3_HUGE_MASK (PML2_HUGE_MASK | (MOS_PLATFORM_PML2_MASK << MOS_PLATFORM_PML2_SHIFT))
#endif
#else
new_named_opaque_type(pml2_t, next, pml3_t);
typedef pml2e_t pml3e_t;
#endif

#if MOS_PLATFORM_PAGING_LEVELS >= 4
define_pmlx(pml4);
#define pml4_index(vaddr) ((vaddr >> MOS_PLATFORM_PML4_SHIFT) & MOS_PLATFORM_PML4_MASK)
#if MOS_CONFIG(MOS_PLATFORM_PML4_HUGE_CAPABLE)
#define PML4_HUGE_MASK (PML3_HUGE_MASK | (MOS_PLATFORM_PML3_MASK << MOS_PLATFORM_PML3_SHIFT))
#endif
#else
new_named_opaque_type(pml3_t, next, pml4_t);
typedef pml3e_t pml4e_t;
#endif

#if MOS_PLATFORM_PAGING_LEVELS >= 5
define_pmlx(pml5);
#else
new_named_opaque_type(pml4_t, next, pml5_t);
typedef pml4e_t pml5e_t;
#endif

typedef struct
{
    MOS_CONCAT(MOS_CONCAT(pml, MOS_MAX_PAGE_LEVEL), _t) max;
} pgd_t;

#define pgd_create(top) ((pgd_t){ .max = { .next = top } })

typedef struct
{
    bool readonly;
    void (*pml1_callback)(pml1_t pml1, pml1e_t *e, ptr_t vaddr, void *data);
#if MOS_PLATFORM_PAGING_LEVELS >= 2
    void (*pml2_callback)(pml2_t pml2, pml2e_t *e, ptr_t vaddr, void *data);
#endif
#if MOS_PLATFORM_PAGING_LEVELS >= 3
    void (*pml3_callback)(pml3_t pml3, pml3e_t *e, ptr_t vaddr, void *data);
#endif
#if MOS_PLATFORM_PAGING_LEVELS >= 4
    void (*pml4_callback)(pml4_t pml4, pml4e_t *e, ptr_t vaddr, void *data);
#endif
} pagetable_walk_options_t;

#define pml_create_table(x) ((x##_t){ .table = (x##e_t *) phyframe_va(mm_get_free_page()) })
