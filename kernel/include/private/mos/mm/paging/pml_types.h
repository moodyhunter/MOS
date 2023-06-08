// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform_defs.h"

#include <mos/mos_global.h>

#define MOS_MAX_PAGE_LEVEL 3

#if MOS_PLATFORM_PAGING_LEVELS >= 4
#error "PML4 is not supported"
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

// pmlmax_t is the top-level paging table supported by MOS
typedef MOS_CONCAT(MOS_CONCAT(pml, MOS_PLATFORM_PAGING_LEVELS), _t) pmltop_t;

typedef struct
{
    MOS_CONCAT(MOS_CONCAT(pml, MOS_MAX_PAGE_LEVEL), _t) real_max;
    pmltop_t platform_top;
    pfn_t pfn;
} pmlmax_t;

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
#endif

#if MOS_PLATFORM_PAGING_LEVELS >= 3
should_inline size_t pml3_index(ptr_t vaddr)
{
    return (vaddr >> MOS_PLATFORM_PML3_SHIFT) & MOS_PLATFORM_PML3_MASK;
}
#endif

typedef struct
{
    bool readonly;
    void (*pml1_callback)(pml1_t pml1, pml1e_t *e, ptr_t vaddr, void *data);
    void (*pml2_callback)(pml2_t pml2, pml2e_t *e, ptr_t vaddr, void *data);
    void (*pml3_callback)(pml3_t pml3, pml3e_t *e, ptr_t vaddr, void *data);
} pagetable_walk_options_t;
