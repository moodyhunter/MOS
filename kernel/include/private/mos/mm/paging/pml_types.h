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

#if MOS_PLATFORM_PAGING_LEVELS >= 2
define_pmlx(pml2);
#if !defined(MOS_PLATFORM_PML2_SHIFT) || !defined(MOS_PLATFORM_PML2_MASK)
#error "MOS_PLATFORM_PML2_SHIFT and MOS_PLATFORM_PML2_MASK must be defined"
#endif
#else
new_named_opaque_type(pml1_t, pml1, pml2_t);
#endif

#if MOS_PLATFORM_PAGING_LEVELS >= 3
define_pmlx(pml3);
#if !defined(MOS_PLATFORM_PML3_SHIFT) || !defined(MOS_PLATFORM_PML3_MASK)
#error "MOS_PLATFORM_PML3_SHIFT and MOS_PLATFORM_PML3_MASK must be defined"
#endif
#else
new_named_opaque_type(pml2_t, pml2, pml3_t);
typedef pml2e_t pml3e_t;
#endif

#if MOS_PLATFORM_PAGING_LEVELS >= 4
define_pmlx(pml4);
#if !defined(MOS_PLATFORM_PML4_SHIFT) || !defined(MOS_PLATFORM_PML4_MASK)
#error "MOS_PLATFORM_PML4_SHIFT and MOS_PLATFORM_PML4_MASK must be defined"
#endif
#else
new_named_opaque_type(pml3_t, pml3, pml4_t);
typedef pml3e_t pml4e_t;
#endif

#if MOS_PLATFORM_PAGING_LEVELS >= 5
define_pmlx(pml5);
#if !defined(MOS_PLATFORM_PML5_SHIFT) || !defined(MOS_PLATFORM_PML5_MASK)
#error "MOS_PLATFORM_PML5_SHIFT and MOS_PLATFORM_PML5_MASK must be defined"
#endif
#else
new_named_opaque_type(pml4_t, pml4, pml5_t);
typedef pml4e_t pml5e_t;
#endif

// pmltop_t is the top-level paging table supported by the platform
typedef MOS_CONCAT(MOS_CONCAT(pml, MOS_PLATFORM_PAGING_LEVELS), _t) pmltop_t;

#define pml_create_table(x)                                                                                                                                              \
    __extension__({                                                                                                                                                      \
        phyframe_t *page = mm_get_free_page();                                                                                                                           \
        (x##_t){ .table = (void *) phyframe_va(page) };                                                                                                                  \
    })

typedef struct
{
    MOS_CONCAT(MOS_CONCAT(pml, MOS_MAX_PAGE_LEVEL), _t) max;
} pgd_t;

/**
 * @brief Define stub functions for platforms that don't support the given paging level
 *
 */
#define define_stub_pml_functions(_this, _parent)                                                                                                                        \
    _this##_t _this##_allocate(void)                                                                                                                                     \
    {                                                                                                                                                                    \
        return (_this##_t){ 0 };                                                                                                                                         \
    }                                                                                                                                                                    \
    void _this##_free(_this##_t _this)                                                                                                                                   \
    {                                                                                                                                                                    \
        MOS_UNUSED(_this);                                                                                                                                               \
    }                                                                                                                                                                    \
    _this##_t _this##_get(_parent##_t parent, ptr_t vaddr)                                                                                                               \
    {                                                                                                                                                                    \
        MOS_UNUSED(parent);                                                                                                                                              \
        MOS_UNUSED(vaddr);                                                                                                                                               \
        MOS_UNREACHABLE();                                                                                                                                               \
    }                                                                                                                                                                    \
    bool _this##_is_present(_this##_t _this, ptr_t vaddr)                                                                                                                \
    {                                                                                                                                                                    \
        MOS_UNUSED(_this);                                                                                                                                               \
        MOS_UNUSED(vaddr);                                                                                                                                               \
        return true;                                                                                                                                                     \
    }

should_inline size_t pml1_index(ptr_t vaddr)
{
    return (vaddr >> MOS_PLATFORM_PML1_SHIFT) & MOS_PLATFORM_PML1_MASK;
}

#if MOS_PLATFORM_PAGING_LEVELS >= 2
should_inline size_t pml2_index(ptr_t vaddr)
{
    return (vaddr >> MOS_PLATFORM_PML2_SHIFT) & MOS_PLATFORM_PML2_MASK;
}
#if MOS_CONFIG(MOS_PLATFORM_PML2_HUGE_CAPABLE)
#define PML2_HUGE_MASK (MOS_PLATFORM_PML1_MASK << MOS_PLATFORM_PML1_SHIFT)
#endif
#endif

#if MOS_PLATFORM_PAGING_LEVELS >= 3
should_inline size_t pml3_index(ptr_t vaddr)
{
    return (vaddr >> MOS_PLATFORM_PML3_SHIFT) & MOS_PLATFORM_PML3_MASK;
}
#if MOS_CONFIG(MOS_PLATFORM_PML3_HUGE_CAPABLE)
#define PML3_HUGE_MASK (PML2_HUGE_MASK | (MOS_PLATFORM_PML2_MASK << MOS_PLATFORM_PML2_SHIFT))
#endif
#endif

#if MOS_PLATFORM_PAGING_LEVELS >= 4
should_inline size_t pml4_index(ptr_t vaddr)
{
    return (vaddr >> MOS_PLATFORM_PML4_SHIFT) & MOS_PLATFORM_PML4_MASK;
}
#if MOS_CONFIG(MOS_PLATFORM_PML4_HUGE_CAPABLE)
#define PML4_HUGE_MASK (PML3_HUGE_MASK | (MOS_PLATFORM_PML3_MASK << MOS_PLATFORM_PML3_SHIFT))
#endif
#endif

typedef struct
{
    bool readonly;
    void (*pml1_callback)(pml1_t pml1, pml1e_t *e, ptr_t vaddr, void *data);
    void (*pml2_callback)(pml2_t pml2, pml2e_t *e, ptr_t vaddr, void *data);
    void (*pml3_callback)(pml3_t pml3, pml3e_t *e, ptr_t vaddr, void *data);
    void (*pml4_callback)(pml4_t pml4, pml4e_t *e, ptr_t vaddr, void *data);
} pagetable_walk_options_t;
