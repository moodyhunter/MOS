// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/mm.h"
#include "mos/mm/paging/pmlx/pml3.h"
#include "mos/mm/paging/pmlx/pml4.h"
#include "mos/platform/platform.h"
#include "mos/riscv64/devices/sbi_console.h"

#include <mos_stdlib.h>

static mos_platform_info_t riscv64_platform_info;
mos_platform_info_t *const platform_info = &riscv64_platform_info;

static mos_platform_info_t riscv64_platform_info = {
    .num_cpus = 1,
    .boot_console = &sbi_console,
};

void platform_startup_early()
{
}

void platform_startup_setup_kernel_mm()
{
    // SV39 supports 1 GB pages
    const size_t STEP = 1 GB / MOS_PAGE_SIZE;
    const size_t total_npages = ALIGN_UP(platform_info->max_pfn, STEP);

    for (pfn_t pfn = 0; pfn < total_npages; pfn += STEP)
    {
        const ptr_t vaddr = pfn_va(pfn);
        pml4e_t *pml4e = pml4_entry(platform_info->kernel_mm->pgd.max.next, vaddr);

        // GB pages are at pml3e level
        const pml3_t pml3 = pml4e_get_or_create_pml3(pml4e);
        pml3e_t *const pml3e = pml3_entry(pml3, vaddr);
        platform_pml3e_set_huge(pml3e, pfn);
        platform_pml3e_set_flags(pml3e, VM_READ | VM_WRITE | VM_GLOBAL);
    }
}

void platform_startup_late()
{
}
