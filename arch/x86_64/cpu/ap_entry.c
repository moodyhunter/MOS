// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/table_ops.h"
#include "mos/x86/descriptors/descriptors.h"

#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/tasks/schedule.h>
#include <mos/x86/cpu/cpuid.h>
#include <mos/x86/interrupt/apic.h>
#include <mos/x86/mm/paging_impl.h>
#include <mos/x86/x86_platform.h>

noreturn void ap_begin_exec(void)
{
    x86_init_current_cpu_gdt();
    x86_init_current_cpu_tss();
    x86_idt_flush();
    x86_enable_paging_impl(pgd_pfn(platform_info->kernel_mm->pgd) * MOS_PAGE_SIZE);

    lapic_enable();

    processor_version_t info;
    cpuid_get_processor_info(&info);

    pr_info("smp: AP %u started", info.ebx.local_apic_id);
    cpuid_print_cpu_info();

    per_cpu(x86_platform.cpu)->id = info.ebx.local_apic_id;
    current_cpu->mm_context = x86_platform.kernel_mm;

    u8 lapic_id = lapic_get_id();
    if (lapic_id != info.ebx.local_apic_id)
        mos_warn("smp: AP %u: LAPIC ID mismatch: %u != %u", info.ebx.local_apic_id, lapic_id, info.ebx.local_apic_id);

    scheduler();
    MOS_UNREACHABLE();
}
