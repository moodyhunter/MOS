// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/mm.h"

#include "mos/platform/platform.h"

#include <mos/mos_global.h>
#include <mos/types.h>

#define make_satp(mode, asid, ppn) ((u64) (mode) << 60 | ((u64) (asid) << 44) | (ppn))

#define SATP_MODE_SV39 8
#define SATP_MODE_SV48 9
#define SATP_MODE_SV57 10

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
    vm_flags flags = 0;
    flags |= pte->r ? VM_READ : 0;
    flags |= pte->w ? VM_WRITE : 0;
    flags |= pte->x ? VM_EXEC : 0;
    flags |= pte->user ? VM_USER : 0;
    flags |= pte->global ? VM_GLOBAL : 0;
    return flags;
}

// Platform CPU APIs
noreturn void platform_halt_cpu(void)
{
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
    return cast_to(pml1e, pml1e_t *, sv48_pte_t *)->ppn;
}

void platform_pml1e_set_pfn(pml1e_t *pml1, pfn_t pfn) // -- which can be a pfn for either a page or another page table
{
    pml1->content = 0;
    sv48_pte_t *pte = cast_to(pml1, pml1e_t *, sv48_pte_t *);
    pte_set_ppn_stem(pte, pfn);
}

bool platform_pml1e_get_present(const pml1e_t *pml1) // returns if an entry in this page table is present
{
    return cast_to(pml1, pml1e_t *, sv48_pte_t *)->valid;
}

void platform_pml1e_set_flags(pml1e_t *pml1, vm_flags flags) // set bits in the flags field of the pmlx entry
{
    sv48_pte_t *pte = cast_to(pml1, pml1e_t *, sv48_pte_t *);
    pmle_set_flags(1, pte, flags);
}

vm_flags platform_pml1e_get_flags(const pml1e_t *pml1e) // get bits in the flags field of the pmlx entry
{
    const sv48_pte_t *pte = cast_to(pml1e, pml1e_t *, sv48_pte_t *);
    return pte_get_flags(pte);
}

pml1_t platform_pml2e_get_pml1(const pml2e_t *pml2)
{
    return (pml1_t){ .table = (pml1e_t *) pfn_va(cast_to(pml2, pml2e_t *, sv48_pte_t *)->ppn) };
}

void platform_pml2e_set_pml1(pml2e_t *pml2, pml1_t pml1, pfn_t pml1_pfn)
{
    MOS_UNUSED(pml1);
    pml2->content = 0;
    sv48_pte_t *pte = cast_to(pml2, pml2e_t *, sv48_pte_t *);
    pte_set_ppn_stem(pte, pml1_pfn);
}

bool platform_pml2e_get_present(const pml2e_t *pml2)
{
    return cast_to(pml2, pml2e_t *, sv48_pte_t *)->valid;
}

void platform_pml2e_set_flags(pml2e_t *pml2, vm_flags flags)
{
    sv48_pte_t *pte = cast_to(pml2, pml2e_t *, sv48_pte_t *);
    pmle_set_flags(2, pte, flags);
}

vm_flags platform_pml2e_get_flags(const pml2e_t *pml2e)
{
    const sv48_pte_t *pte = cast_to(pml2e, pml2e_t *, sv48_pte_t *);
    return pte_get_flags(pte);
}

#if MOS_CONFIG(PML2_HUGE_CAPABLE)
bool platform_pml2e_is_huge(const pml2e_t *pml2)
{
    const sv48_pte_t *pte = cast_to(pml2, pml2e_t *, sv48_pte_t *);
    return pte_is_huge(pte);
}

void platform_pml2e_set_huge(pml2e_t *pml2, pfn_t pfn)
{
    pml2->content = 0;
    sv48_pte_t *pte = cast_to(pml2, pml2e_t *, sv48_pte_t *);
    pte_set_ppn_huge(pte, pfn);
}

pfn_t platform_pml2e_get_huge_pfn(const pml2e_t *pml2)
{
    return cast_to(pml2, pml2e_t *, sv48_pte_t *)->ppn;
}
#endif

pml2_t platform_pml3e_get_pml2(const pml3e_t *pml3)
{
    return (pml2_t){ .table = (pml2e_t *) pfn_va(cast_to(pml3, pml3e_t *, sv48_pte_t *)->ppn) };
}

void platform_pml3e_set_pml2(pml3e_t *pml3, pml2_t pml2, pfn_t pml2_pfn)
{
    MOS_UNUSED(pml2);
    pml3->content = 0;
    sv48_pte_t *pte = cast_to(pml3, pml3e_t *, sv48_pte_t *);
    pte_set_ppn_stem(pte, pml2_pfn);
}

bool platform_pml3e_get_present(const pml3e_t *pml3)
{
    return cast_to(pml3, pml3e_t *, sv48_pte_t *)->valid;
}

void platform_pml3e_set_flags(pml3e_t *pml3, vm_flags flags)
{
    sv48_pte_t *pte = cast_to(pml3, pml3e_t *, sv48_pte_t *);
    pmle_set_flags(3, pte, flags);
}

vm_flags platform_pml3e_get_flags(const pml3e_t *pml3e)
{
    const sv48_pte_t *pte = cast_to(pml3e, pml3e_t *, sv48_pte_t *);
    return pte_get_flags(pte);
}

#if MOS_CONFIG(PML3_HUGE_CAPABLE)
bool platform_pml3e_is_huge(const pml3e_t *pml3)
{
    const sv48_pte_t *pte = cast_to(pml3, pml3e_t *, sv48_pte_t *);
    return pte_is_huge(pte);
}

void platform_pml3e_set_huge(pml3e_t *pml3, pfn_t pfn)
{
    pml3->content = 0;
    sv48_pte_t *pte = cast_to(pml3, pml3e_t *, sv48_pte_t *);
    pte_set_ppn_huge(pte, pfn);
}

pfn_t platform_pml3e_get_huge_pfn(const pml3e_t *pml3)
{
    return cast_to(pml3, pml3e_t *, sv48_pte_t *)->ppn;
}
#endif

pml3_t platform_pml4e_get_pml3(const pml4e_t *pml4)
{
    return (pml3_t){ .table = (pml3e_t *) pfn_va(cast_to(pml4, pml4e_t *, sv48_pte_t *)->ppn) };
}

void platform_pml4e_set_pml3(pml4e_t *pml4, pml3_t pml3, pfn_t pml3_pfn)
{
    MOS_UNUSED(pml3);
    pml4->content = 0;
    sv48_pte_t *pte = cast_to(pml4, pml4e_t *, sv48_pte_t *);
    pte_set_ppn_stem(pte, pml3_pfn);
}

bool platform_pml4e_get_present(const pml4e_t *pml4)
{
    return cast_to(pml4, pml4e_t *, sv48_pte_t *)->valid;
}

void platform_pml4e_set_flags(pml4e_t *pml4, vm_flags flags)
{
    sv48_pte_t *pte = cast_to(pml4, pml4e_t *, sv48_pte_t *);
    pmle_set_flags(4, pte, flags);
}

vm_flags platform_pml4e_get_flags(const pml4e_t *pml4e)
{
    const sv48_pte_t *pte = cast_to(pml4e, pml4e_t *, sv48_pte_t *);
    return pte_get_flags(pte);
}

#if MOS_CONFIG(PML4_HUGE_CAPABLE)
bool platform_pml4e_is_huge(const pml4e_t *pml4)
{
    const sv48_pte_t *pte = cast_to(pml4, pml4e_t *, sv48_pte_t *);
    return pte_is_huge(pte);
}

void platform_pml4e_set_huge(pml4e_t *pml4, pfn_t pfn)
{
    pml4->content = 0;
    sv48_pte_t *pte = cast_to(pml4, pml4e_t *, sv48_pte_t *);
    pte_set_ppn_huge(pte, pfn);
}

pfn_t platform_pml4e_get_huge_pfn(const pml4e_t *pml4)
{
    return cast_to(pml4, pml4e_t *, sv48_pte_t *)->ppn;
}
#endif

void platform_switch_mm(const mm_context_t *new_mm)
{
    __asm__ volatile("csrw satp, %0" : : "r"(make_satp(SATP_MODE_SV48, 0, pgd_pfn(new_mm->pgd))) : "memory");
    __asm__ volatile("sfence.vma zero, zero");
}
