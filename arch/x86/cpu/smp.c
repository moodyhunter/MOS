// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/cpu/smp.h"

#include "lib/string.h"
#include "mos/boot/startup.h"
#include "mos/kconfig.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/x86/acpi/acpi.h"
#include "mos/x86/cpu/cpuid.h"
#include "mos/x86/delays.h"
#include "mos/x86/interrupt/apic.h"
#include "mos/x86/interrupt/pic.h"
#include "mos/x86/mm/paging_impl.h"
#include "mos/x86/x86_platform.h"

#define X86_AP_TRAMPOLINE_ADDR 0x8000

volatile enum
{
    AP_STATUS_INVALID = 0,
    AP_STATUS_BSP_STARTUP_SENT = 1,
    AP_STATUS_AP_WAIT_FOR_STACK_ALLOC = 2,
    AP_STATUS_STACK_ALLOCATED = 3,
    AP_STATUS_STACK_INITIALIZED = 4,
    AP_STATUS_START = 5,
    AP_STATUS_STARTED = 6,
} ap_state;

volatile uintptr_t ap_stack_addr = 0;
static u32 lapics[MOS_MAX_CPU_COUNT] = { 0 };
extern void x86_ap_trampoline();

void print_cpu_info()
{
    char manufacturer[13];
    cpuid_get_manufacturer(manufacturer);
    processor_version_t info;
    cpuid_get_processor_info(&info);
    pr_info2("  %s, Family %u, Model %u, Stepping %u", manufacturer, info.eax.eax.family, info.eax.eax.model, info.eax.eax.stepping);
    pr_info2("  type: %s, ext family: %u, ext model: %u", cpuid_type_str[info.eax.eax.type], info.eax.eax.ext_family, info.eax.eax.ext_model);
}

void ap_begin_exec()
{
    x86_ap_gdt_init();
    x86_idt_init();

    processor_version_t info;
    cpuid_get_processor_info(&info);

    pr_info("smp: AP %u started", info.ebx.ebx.local_apic_id);
    print_cpu_info();

    per_cpu(x86_platform.cpu)->id = info.ebx.ebx.local_apic_id;

    // TODO: enable paging
    while (1)
        ;
}

// clang-format off
#define wait_for(state) do {} while (ap_state != state)
// clang-format on

void x86_cpu_start(int apic_id, uintptr_t stack_addr)
{
    ap_state = AP_STATUS_INVALID;
    apic_interrupt_full(0, apic_id, APIC_DELIVER_MODE_INIT, APIC_DEST_MODE_PHYSICAL, true, true, APIC_SHORTHAND_NONE);
    mdelay(100);
    apic_interrupt_full(0, apic_id, APIC_DELIVER_MODE_INIT_DEASSERT, APIC_DEST_MODE_PHYSICAL, false, true, APIC_SHORTHAND_NONE);

    ap_state = AP_STATUS_BSP_STARTUP_SENT;
    pr_info2("smp: bsp sent startup to cpu %u", apic_id);

    mdelay(100);
    apic_interrupt(X86_AP_TRAMPOLINE_ADDR >> 12, apic_id, APIC_DELIVER_MODE_STARTUP, APIC_DEST_MODE_PHYSICAL, APIC_SHORTHAND_NONE);
    mdelay(100);
    apic_interrupt(X86_AP_TRAMPOLINE_ADDR >> 12, apic_id, APIC_DELIVER_MODE_STARTUP, APIC_DEST_MODE_PHYSICAL, APIC_SHORTHAND_NONE);

    wait_for(AP_STATUS_AP_WAIT_FOR_STACK_ALLOC);
    pr_info2("smp: received startup from cpu %u", apic_id);

    // allocate stack for ap
    ap_stack_addr = stack_addr;
    ap_state = AP_STATUS_STACK_ALLOCATED;
    wait_for(AP_STATUS_STACK_INITIALIZED);
    pr_info2("smp: initialized stack for cpu %u", apic_id);

    ap_state = AP_STATUS_START;
    wait_for(AP_STATUS_STARTED);
    pr_info2("smp: started cpu %u", apic_id);
}

#undef wait_for

void x86_smp_init(x86_pg_infra_t *pg)
{
    pr_info("smp: boot cpu:");
    print_cpu_info();

    pr_info2("disabling 8259 PIC...");
    pic_disable();

    pr_info2("enabling APIC...");
    apic_enable();

    u32 num_cpus = 0;
    processor_version_t info;
    cpuid_get_processor_info(&info);
    x86_platform.boot_cpu_id = info.ebx.ebx.local_apic_id;
    per_cpu(x86_platform.cpu)->id = x86_platform.boot_cpu_id;

    madt_entry_foreach(entry, x86_acpi_madt)
    {
        if (entry->type == 0)
        {
            acpi_madt_et0_lapic_t *et0 = (acpi_madt_et0_lapic_t *) entry;
            pr_info2("cpu %d: processor id %d, apic id %d", num_cpus, et0->processor_id, et0->apic_id);
            lapics[num_cpus] = et0->apic_id;
            num_cpus++;
        }
    }

    pr_info("smp: platform has %u cpu(s)", num_cpus);
    x86_platform.num_cpus = num_cpus;

    mos_startup_map_pages(X86_AP_TRAMPOLINE_ADDR, X86_AP_TRAMPOLINE_ADDR, 4 KB / MOS_PAGE_SIZE, VM_WRITE);
    memcpy((void *) X86_AP_TRAMPOLINE_ADDR, (void *) (uintptr_t) &x86_ap_trampoline, 4 KB);

    for (u32 i = 0; i < num_cpus; i++)
    {
        u32 apic_id = lapics[i];
        if (apic_id == x86_platform.boot_cpu_id)
            continue;

        uintptr_t stack = (uintptr_t) pg_page_alloc(pg, 4, PGALLOC_NONE);
        pr_info("smp: starting AP %d, LAPIC %d, stack " PTR_FMT, i, apic_id, stack);
        x86_cpu_start(apic_id, stack);
    }
}
