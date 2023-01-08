// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/printk.h"
#include "mos/x86/cpu/cpuid.h"
#include "mos/x86/interrupt/apic.h"
#include "mos/x86/mm/paging.h"
#include "mos/x86/x86_platform.h"

void ap_begin_exec()
{
    x86_ap_gdt_init();
    x86_idt_init();
    x86_mm_enable_paging();
    lapic_enable();

    processor_version_t info;
    cpuid_get_processor_info(&info);

    pr_info("smp: AP %u started", info.ebx.local_apic_id);
    cpuid_print_cpu_info();

    per_cpu(x86_platform.cpu)->id = info.ebx.local_apic_id;
    u8 lapic_id = lapic_get_id();
    if (lapic_id != info.ebx.local_apic_id)
        mos_warn("smp: AP %u: LAPIC ID mismatch: %u != %u", info.ebx.local_apic_id, lapic_id, info.ebx.local_apic_id);

    while (1)
        __asm__ volatile("cli; hlt;");
}
