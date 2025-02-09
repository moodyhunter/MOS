// SPDX-License-Identifier: GPL-3.0-or-later

#define pr_fmt(fmt) "ap_entry: " fmt

#include "mos/x86/cpu/ap_entry.hpp"

#include "mos/platform/platform.hpp"
#include "mos/syslog/printk.hpp"
#include "mos/tasks/schedule.hpp"
#include "mos/x86/cpu/cpu.hpp"
#include "mos/x86/descriptors/descriptors.hpp"
#include "mos/x86/interrupt/apic.hpp"
#include "mos/x86/x86_platform.hpp"

static bool aps_blocked = true;

void x86_unblock_aps(void)
{
    MOS_ASSERT(aps_blocked);
    aps_blocked = false;
}

void platform_ap_entry(u64 arg)
{
    MOS_UNUSED(arg);

#if !MOS_CONFIG(MOS_SMP)
    pr_info("SMP not enabled, halting AP");
    platform_halt_cpu();
#endif

    while (aps_blocked)
        __asm__ volatile("pause");

    x86_init_percpu_gdt();
    x86_init_percpu_tss();
    x86_init_percpu_idt();

    // enable paging
    platform_switch_mm(platform_info->kernel_mm);

    x86_cpu_initialise_caps();
    x86_cpu_setup_xsave_area();
    lapic_enable();

    const u8 processor_id = platform_current_cpu_id();
    pr_dinfo2(x86_startup, "AP %u started", processor_id);

    const u8 lapic_id = lapic_get_id();
    if (lapic_id != processor_id)
        pr_warn("LAPIC ID mismatch: LAPIC_ID: %u != PROCESSOR_ID: %u", lapic_id, processor_id);

    current_cpu->mm_context = platform_info->kernel_mm;
    current_cpu->id = lapic_id;

    x86_setup_lapic_timer();
    enter_scheduler();
}
