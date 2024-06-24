// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/assert.h"
#include "mos/platform/platform.h"

MOS_STUB_IMPL(platform_regs_t *platform_thread_regs(const thread_t *thread))

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
MOS_STUB_IMPL(pfn_t platform_pml1e_get_pfn(const pml1e_t *pml1))            // returns the physical address contained in the pmlx entry,
MOS_STUB_IMPL(void platform_pml1e_set_pfn(pml1e_t *pml1, pfn_t pfn))        // -- which can be a pfn for either a page or another page table
MOS_STUB_IMPL(bool platform_pml1e_get_present(const pml1e_t *pml1))         // returns if an entry in this page table is present
MOS_STUB_IMPL(void platform_pml1e_set_present(pml1e_t *pml1, bool present)) // sets if an entry in this page table is present
MOS_STUB_IMPL(void platform_pml1e_set_flags(pml1e_t *pml1, vm_flags flags)) // set bits in the flags field of the pmlx entry
MOS_STUB_IMPL(vm_flags platform_pml1e_get_flags(const pml1e_t *pml1e))      // get bits in the flags field of the pmlx entry

#if MOS_PLATFORM_PAGING_LEVELS >= 2
MOS_STUB_IMPL(pml1_t platform_pml2e_get_pml1(const pml2e_t *pml2))
MOS_STUB_IMPL(void platform_pml2e_set_pml1(pml2e_t *pml2, pml1_t pml1, pfn_t pml1_pfn))
MOS_STUB_IMPL(bool platform_pml2e_get_present(const pml2e_t *pml2))
MOS_STUB_IMPL(void platform_pml2e_set_present(pml2e_t *pml2, bool present))
MOS_STUB_IMPL(void platform_pml2e_set_flags(pml2e_t *pml2, vm_flags flags))
MOS_STUB_IMPL(vm_flags platform_pml2e_get_flags(const pml2e_t *pml2e))
#if MOS_CONFIG(PML2_HUGE_CAPABLE)
MOS_STUB_IMPL(bool platform_pml2e_is_huge(const pml2e_t *pml2))
MOS_STUB_IMPL(void platform_pml2e_set_huge(pml2e_t *pml2, pfn_t pfn))
MOS_STUB_IMPL(pfn_t platform_pml2e_get_huge_pfn(const pml2e_t *pml2))
#endif
#endif

#if MOS_PLATFORM_PAGING_LEVELS >= 3
MOS_STUB_IMPL(pml2_t platform_pml3e_get_pml2(const pml3e_t *pml3))
MOS_STUB_IMPL(void platform_pml3e_set_pml2(pml3e_t *pml3, pml2_t pml2, pfn_t pml2_pfn))
MOS_STUB_IMPL(bool platform_pml3e_get_present(const pml3e_t *pml3))
MOS_STUB_IMPL(void platform_pml3e_set_present(pml3e_t *pml3, bool present))
MOS_STUB_IMPL(void platform_pml3e_set_flags(pml3e_t *pml3, vm_flags flags))
MOS_STUB_IMPL(vm_flags platform_pml3e_get_flags(const pml3e_t *pml3e))
#if MOS_CONFIG(PML3_HUGE_CAPABLE)
MOS_STUB_IMPL(bool platform_pml3e_is_huge(const pml3e_t *pml3))
MOS_STUB_IMPL(void platform_pml3e_set_huge(pml3e_t *pml3, pfn_t pfn))
MOS_STUB_IMPL(pfn_t platform_pml3e_get_huge_pfn(const pml3e_t *pml3))
#endif
#endif

#if MOS_PLATFORM_PAGING_LEVELS >= 4
MOS_STUB_IMPL(pml3_t platform_pml4e_get_pml3(const pml4e_t *pml4))
MOS_STUB_IMPL(void platform_pml4e_set_pml3(pml4e_t *pml4, pml3_t pml3, pfn_t pml3_pfn))
MOS_STUB_IMPL(bool platform_pml4e_get_present(const pml4e_t *pml4))
MOS_STUB_IMPL(void platform_pml4e_set_present(pml4e_t *pml4, bool present))
MOS_STUB_IMPL(void platform_pml4e_set_flags(pml4e_t *pml4, vm_flags flags))
MOS_STUB_IMPL(vm_flags platform_pml4e_get_flags(const pml4e_t *pml4e))
#if MOS_CONFIG(PML4_HUGE_CAPABLE)
MOS_STUB_IMPL(bool platform_pml4e_is_huge(const pml4e_t *pml4))
MOS_STUB_IMPL(void platform_pml4e_set_huge(pml4e_t *pml4, pfn_t pfn))
MOS_STUB_IMPL(pfn_t platform_pml4e_get_huge_pfn(const pml4e_t *pml4))
#endif
#endif

// Platform Thread / Process APIs
MOS_STUB_IMPL(void platform_context_setup_main_thread(thread_t *thread, ptr_t entry, ptr_t sp, int argc, ptr_t argv, ptr_t envp))
MOS_STUB_IMPL(void platform_context_setup_child_thread(thread_t *thread, thread_entry_t entry, void *arg))
MOS_STUB_IMPL(void platform_context_clone(const thread_t *from, thread_t *to))
MOS_STUB_IMPL(void platform_context_cleanup(thread_t *thread))

// Platform Context Switching APIs
MOS_STUB_IMPL(void platform_switch_mm(const mm_context_t *new_mm))
MOS_STUB_IMPL(void platform_switch_to_thread(ptr_t *old_stack, thread_t *new_thread, switch_flags_t switch_flags))
MOS_STUB_IMPL(void platform_switch_to_scheduler(ptr_t *old_stack, ptr_t new_stack))
MOS_STUB_IMPL(noreturn void platform_return_to_userspace(platform_regs_t *regs))
