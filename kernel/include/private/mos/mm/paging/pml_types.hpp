// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform_defs.hpp"

#include <mos/mos_global.h>
#include <mos/types.hpp>

#define MOS_MAX_PAGE_LEVEL 5

#ifndef MOS_PLATFORM_PAGING_LEVELS
#error "MOS_PLATFORM_PAGING_LEVELS must be defined"
#endif

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

#define pml_null(pmln) (pmln.table == NULL)

// nah, your platform must have at least 1 level of paging
define_pmlx(pml1);

#define pml1_index(vaddr) ((vaddr >> PML1_SHIFT) & PML1_MASK)
#define PML1E_NPAGES      1ULL

#if MOS_PLATFORM_PAGING_LEVELS >= 2
define_pmlx(pml2);
#define pml2_index(vaddr) ((vaddr >> PML2_SHIFT) & PML2_MASK)
#define PML2E_NPAGES      (PML1_ENTRIES * PML1E_NPAGES)
#if MOS_CONFIG(PML2_HUGE_CAPABLE)
#define PML2_HUGE_MASK (PML1_MASK << PML1_SHIFT)
#endif
#else
new_named_opaque_type(pml1_t, next, pml2_t);
#endif

#if MOS_PLATFORM_PAGING_LEVELS >= 3
define_pmlx(pml3);
#define pml3_index(vaddr) ((vaddr >> PML3_SHIFT) & PML3_MASK)
#define PML3E_NPAGES      (PML2_ENTRIES * PML2E_NPAGES)
#if MOS_CONFIG(PML3_HUGE_CAPABLE)
#define PML3_HUGE_MASK (PML2_HUGE_MASK | (PML2_MASK << PML2_SHIFT))
#endif
#else
new_named_opaque_type(pml2_t, next, pml3_t);
typedef pml2e_t pml3e_t;
#endif

#if MOS_PLATFORM_PAGING_LEVELS >= 4
define_pmlx(pml4);
#define pml4_index(vaddr) ((vaddr >> PML4_SHIFT) & PML4_MASK)
#define PML4E_NPAGES      (PML3_ENTRIES * PML3E_NPAGES)
#if MOS_CONFIG(PML4_HUGE_CAPABLE)
#define PML4_HUGE_MASK (PML3_HUGE_MASK | (PML3_MASK << PML3_SHIFT))
#endif
#else
new_named_opaque_type(pml3_t, next, pml4_t);
typedef pml3e_t pml4e_t;
#endif

#if MOS_PLATFORM_PAGING_LEVELS >= 5
#error "TODO: more than 4 levels"
#else
new_named_opaque_type(pml4_t, next, pml5_t);
typedef pml4e_t pml5e_t;
#endif

#define MOS_PMLTOP MOS_CONCAT(pml, MOS_PLATFORM_PAGING_LEVELS)

typedef struct
{
    MOS_CONCAT(MOS_CONCAT(pml, MOS_MAX_PAGE_LEVEL), _t) max;
} pgd_t;

#define pgd_create(top)                                                                                                                                                  \
    {                                                                                                                                                                    \
        .max = {.next = top }                                                                                                                                            \
    }
#define pgd_pfn(pgd) va_pfn(pgd.max.next.table)

typedef struct
{
    bool readonly;
    void (*pml4e_pre_traverse)(pml4_t pml4, pml4e_t *e, ptr_t vaddr, void *data);
    void (*pml3e_pre_traverse)(pml3_t pml3, pml3e_t *e, ptr_t vaddr, void *data);
    void (*pml2e_pre_traverse)(pml2_t pml2, pml2e_t *e, ptr_t vaddr, void *data);
    void (*pml1e_callback)(pml1_t pml1, pml1e_t *e, ptr_t vaddr, void *data);
    void (*pml2e_post_traverse)(pml2_t pml2, pml2e_t *e, ptr_t vaddr, void *data);
    void (*pml3e_post_traverse)(pml3_t pml3, pml3e_t *e, ptr_t vaddr, void *data);
    void (*pml4e_post_traverse)(pml4_t pml4, pml4e_t *e, ptr_t vaddr, void *data);
} pagetable_walk_options_t;

__nodiscard void *__create_page_table(void);
void __destroy_page_table(void *table);

#define pml_create_table(x)  ((MOS_CONCAT(x, _t)) { .table = (MOS_CONCAT(x, e_t) *) __create_page_table() })
#define pml_destroy_table(x) __destroy_page_table(x.table)

#define pmlxe_destroy(pmlxe) (pmlxe)->content = 0
