// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/mm.hpp"

#include "mos/platform/platform.hpp"

#include <mos/mos_global.h>
#include <mos/types.hpp>

typedef struct _sv48_pte
{
    bool valid : 1; // Valid

    // When all three are zero, the PTE is a pointer to the next level of the page table;
    // Otherwise, it's a leaf PTE.
    bool r : 1; // Readable
    bool w : 1; // Writable, also must be readable
    bool x : 1; // Executable

    bool user : 1;
    bool global : 1; // Global, For non-leaf PTEs, the global setting implies that all mappings in the subsequent levels of the page table are global.
    bool accessed : 1;
    bool dirty : 1;
    u8 _1 : 2;
    u64 ppn : 44; // Physical page number
    u8 _2 : 7;
    u8 pbmt : 2; // Svpbmt
    bool n : 1;  // Svnapot
} __packed sv48_pte_t;

MOS_STATIC_ASSERT(sizeof(sv48_pte_t) == sizeof(pte_content_t));

#define pte_is_stem(pte) ((pte)->r == 0 && (pte)->w == 0 && (pte)->x == 0 && (pte)->valid)
#define pte_is_huge(pte) (((pte)->r || (pte)->w || (pte)->x) && (pte)->valid)

#define pte_set_ppn_stem(pte, _ppn) ((pte)->valid = 1, (pte)->ppn = _ppn, (pte)->r = 0, (pte)->w = 0, (pte)->x = 0)
#define pte_set_ppn_huge(pte, _ppn) ((pte)->valid = 1, (pte)->ppn = _ppn, (pte)->r = 1, (pte)->w = 0, (pte)->x = 0)

should_inline void pmle_set_flags(int level, sv48_pte_t *pte, vm_flags flags)
{
    if (!pte_is_stem(pte) || level == 1)
    {
        // only set these for leaf ptes, or for huge pages
        pte->r = flags & VM_READ;
        pte->w = flags & VM_WRITE;
        pte->x = flags & VM_EXEC;
        pte->user = flags & VM_USER;
    }

    pte->global = flags & VM_GLOBAL;
}

should_inline vm_flags pte_get_flags(const sv48_pte_t *pte)
{
    vm_flags flags = (vm_flags) 0;
    flags |= pte->r ? VM_READ : 0;
    flags |= pte->w ? VM_WRITE : 0;
    flags |= pte->x ? VM_EXEC : 0;
    flags |= pte->user ? VM_USER : 0;
    flags |= pte->global ? VM_GLOBAL : 0;
    return flags;
}

// Platform CPU APIs
[[noreturn]] void platform_halt_cpu(void)
{
    while (true)
        __asm__ volatile("wfi"); // wait for interrupt
    __builtin_unreachable();
}

void platform_invalidate_tlb(ptr_t vaddr)
{
    __asm__ volatile("sfence.vma %0" : : "r"(vaddr) : "memory");
}

// Platform Page Table APIs
pfn_t platform_pml1e_get_pfn(const pml1e_t *pml1e) // returns the physical address contained in the pmlx entry,
{
    return cast<sv48_pte_t>(pml1e)->ppn;
}

void platform_pml1e_set_pfn(pml1e_t *pml1e, pfn_t pfn) // -- which can be a pfn for either a page or another page table
{
    pml1e->content = 0;
    sv48_pte_t *pte = cast<sv48_pte_t>(pml1e);
    pte_set_ppn_stem(pte, pfn);
}

bool platform_pml1e_get_present(const pml1e_t *pml1e) // returns if an entry in this page table is present
{
    return cast<sv48_pte_t>(pml1e)->valid;
}

void platform_pml1e_set_flags(pml1e_t *pml1e, vm_flags flags) // set bits in the flags field of the pmlx entry
{
    sv48_pte_t *pte = cast<sv48_pte_t>(pml1e);
    pmle_set_flags(1, pte, flags);
}

vm_flags platform_pml1e_get_flags(const pml1e_t *pml1e) // get bits in the flags field of the pmlx entry
{
    const sv48_pte_t *pte = cast<sv48_pte_t>(pml1e);
    return pte_get_flags(pte);
}

pml1_t platform_pml2e_get_pml1(const pml2e_t *pml2e)
{
    return { .table = (pml1e_t *) pfn_va(cast<sv48_pte_t>(pml2e)->ppn) };
}

void platform_pml2e_set_pml1(pml2e_t *pml2e, pml1_t pml1, pfn_t pml1_pfn)
{
    MOS_UNUSED(pml1);
    pml2e->content = 0;
    sv48_pte_t *pte = cast<sv48_pte_t>(pml2e);
    pte_set_ppn_stem(pte, pml1_pfn);
}

bool platform_pml2e_get_present(const pml2e_t *pml2e)
{
    return cast<sv48_pte_t>(pml2e)->valid;
}

void platform_pml2e_set_flags(pml2e_t *pml2e, vm_flags flags)
{
    sv48_pte_t *pte = cast<sv48_pte_t>(pml2e);
    pmle_set_flags(2, pte, flags);
}

vm_flags platform_pml2e_get_flags(const pml2e_t *pml2e)
{
    const sv48_pte_t *pte = cast<sv48_pte_t>(pml2e);
    return pte_get_flags(pte);
}

#if MOS_CONFIG(PML2_HUGE_CAPABLE)
bool platform_pml2e_is_huge(const pml2e_t *pml2e)
{
    const sv48_pte_t *pte = cast<sv48_pte_t>(pml2e);
    return pte_is_huge(pte);
}

void platform_pml2e_set_huge(pml2e_t *pml2e, pfn_t pfn)
{
    pml2e->content = 0;
    sv48_pte_t *pte = cast<sv48_pte_t>(pml2e);
    pte_set_ppn_huge(pte, pfn);
}

pfn_t platform_pml2e_get_huge_pfn(const pml2e_t *pml2e)
{
    return cast<sv48_pte_t>(pml2e)->ppn;
}
#endif

pml2_t platform_pml3e_get_pml2(const pml3e_t *pml3e)
{
    return { .table = (pml2e_t *) pfn_va(cast<sv48_pte_t>(pml3e)->ppn) };
}

void platform_pml3e_set_pml2(pml3e_t *pml3e, pml2_t pml2, pfn_t pml2_pfn)
{
    MOS_UNUSED(pml2);
    pml3e->content = 0;
    sv48_pte_t *pte = cast<sv48_pte_t>(pml3e);
    pte_set_ppn_stem(pte, pml2_pfn);
}

bool platform_pml3e_get_present(const pml3e_t *pml3e)
{
    return cast<sv48_pte_t>(pml3e)->valid;
}

void platform_pml3e_set_flags(pml3e_t *pml3e, vm_flags flags)
{
    sv48_pte_t *pte = cast<sv48_pte_t>(pml3e);
    pmle_set_flags(3, pte, flags);
}

vm_flags platform_pml3e_get_flags(const pml3e_t *pml3e)
{
    const sv48_pte_t *pte = cast<sv48_pte_t>(pml3e);
    return pte_get_flags(pte);
}

#if MOS_CONFIG(PML3_HUGE_CAPABLE)
bool platform_pml3e_is_huge(const pml3e_t *pml3e)
{
    const sv48_pte_t *pte = cast<sv48_pte_t>(pml3e);
    return pte_is_huge(pte);
}

void platform_pml3e_set_huge(pml3e_t *pml3e, pfn_t pfn)
{
    pml3e->content = 0;
    sv48_pte_t *pte = cast<sv48_pte_t>(pml3e);
    pte_set_ppn_huge(pte, pfn);
}

pfn_t platform_pml3e_get_huge_pfn(const pml3e_t *pml3e)
{
    return cast<sv48_pte_t>(pml3e)->ppn;
}
#endif

pml3_t platform_pml4e_get_pml3(const pml4e_t *pml4e)
{
    return { .table = (pml3e_t *) pfn_va(cast<sv48_pte_t>(pml4e)->ppn) };
}

void platform_pml4e_set_pml3(pml4e_t *pml4e, pml3_t pml3, pfn_t pml3_pfn)
{
    MOS_UNUSED(pml3);
    pml4e->content = 0;
    sv48_pte_t *pte = cast<sv48_pte_t>(pml4e);
    pte_set_ppn_stem(pte, pml3_pfn);
}

bool platform_pml4e_get_present(const pml4e_t *pml4e)
{
    return cast<sv48_pte_t>(pml4e)->valid;
}

void platform_pml4e_set_flags(pml4e_t *pml4e, vm_flags flags)
{
    sv48_pte_t *pte = cast<sv48_pte_t>(pml4e);
    pmle_set_flags(4, pte, flags);
}

vm_flags platform_pml4e_get_flags(const pml4e_t *pml4e)
{
    const sv48_pte_t *pte = cast<sv48_pte_t>(pml4e);
    return pte_get_flags(pte);
}

#if MOS_CONFIG(PML4_HUGE_CAPABLE)
bool platform_pml4e_is_huge(const pml4e_t *pml4e)
{
    const sv48_pte_t *pte = cast<sv48_pte_t>(pml4e);
    return pte_is_huge(pte);
}

void platform_pml4e_set_huge(pml4e_t *pml4e, pfn_t pfn)
{
    pml4e->content = 0;
    sv48_pte_t *pte = cast<sv48_pte_t>(pml4e);
    pte_set_ppn_huge(pte, pfn);
}

pfn_t platform_pml4e_get_huge_pfn(const pml4e_t *pml4e)
{
    return cast<sv48_pte_t>(pml4e)->ppn;
}
#endif
