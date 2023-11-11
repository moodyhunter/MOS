// SPDX-License-Identifier: GPL-3.0-or-later

#define pr_fmt(fmt) "ap_entry: " fmt

#include "mos/mm/mm.h"
#include "mos/mm/paging/pml_types.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/schedule.h"
#include "mos/x86/cpu/cpu.h"
#include "mos/x86/descriptors/descriptors.h"
#include "mos/x86/interrupt/apic.h"

static bool aps_blocked = true;

void x86_start_all_aps(void)
{
    MOS_ASSERT(aps_blocked);
    aps_blocked = false;
}

noreturn void x86_ap_begin_exec(void)
{
    while (aps_blocked)
        __asm__ volatile("pause");

    x86_init_percpu_gdt();
    x86_init_percpu_tss();
    x86_init_percpu_idt();

    // enable paging
    x86_cpu_set_cr3(pgd_pfn(platform_info->kernel_mm->pgd) * MOS_PAGE_SIZE);
    __asm__ volatile("mov %%cr4, %%rax; orq $0x80, %%rax; mov %%rax, %%cr4" ::: "rax"); // and enable PGE

    lapic_enable();

    const u8 processor_id = platform_current_cpu_id();
    pr_info2("AP %u started", processor_id);

    const u8 lapic_id = lapic_get_id();
    if (lapic_id != processor_id)
        mos_warn("LAPIC ID mismatch: LAPIC_ID: %u != PROCESSOR_ID: %u", lapic_id, processor_id);

    current_cpu->mm_context = platform_info->kernel_mm;
    current_cpu->id = lapic_id;
    scheduler();
}
