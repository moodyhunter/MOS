// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/cpu/smp.h"

#include "lib/string.h"
#include "mos/boot/startup.h"
#include "mos/mos_global.h"
#include "mos/printk.h"
#include "mos/types.h"
#include "mos/x86/acpi/acpi.h"
#include "mos/x86/acpi/acpi_types.h"
#include "mos/x86/cpu/cpu.h"
#include "mos/x86/cpu/cpuid.h"
#include "mos/x86/delays.h"
#include "mos/x86/interrupt/apic.h"
#include "mos/x86/interrupt/pic.h"
#include "mos/x86/mm/paging.h"
#include "mos/x86/mm/paging_impl.h"
#include "mos/x86/x86_platform.h"

volatile enum
{
    AP_STATUS_INVALID = 0,
    AP_STATUS_BSP_STARTUP_SENT = 1,
    AP_STATUS_START_REQUEST = 2,
    AP_STATUS_START = 3,
} ap_state;

volatile uintptr_t ap_stack_addr = 0;
volatile uintptr_t ap_pgd_addr = 0;
static u32 lapics[MOS_MAX_CPU_COUNT] = { 0 };
extern void x86_ap_trampoline();

void print_cpu_info()
{
    char manufacturer[13];
    cpuid_get_manufacturer(manufacturer);
    processor_version_t info;
    cpuid_get_processor_info(&info);
    char brand_string[49];
    cpuid_get_brand_string(brand_string);
    pr_info2("CPU: %s (%s)", brand_string, manufacturer);
    pr_info2("  Family %u, Model %u, Stepping %u", info.eax.family, info.eax.model, info.eax.stepping);
    pr_info2("  Type: %s, Ext family: %u, Ext model: %u", cpuid_type_str[info.eax.type], info.eax.ext_family, info.eax.ext_model);
}

void ap_begin_exec()
{
    x86_ap_gdt_init();
    x86_idt_init();
    x86_mm_enable_paging();

    processor_version_t info;
    cpuid_get_processor_info(&info);

    pr_info("smp: AP %u started", info.ebx.local_apic_id);
    print_cpu_info();

    per_cpu(x86_platform.cpu)->id = info.ebx.local_apic_id;

    unsigned int x = lapic_reg_read_offset_32(APIC_REG_LAPIC_ID);
    if (x != info.ebx.local_apic_id)
    {
        mos_warn("smp: AP %u: LAPIC ID mismatch: %u != %u", info.ebx.local_apic_id, x, info.ebx.local_apic_id);
    }

    while (1)
        __asm__ volatile("cli; hlt");
}

// clang-format off
#define wait_for(state) do {} while (ap_state != state)
// clang-format on

void x86_cpu_start(int apic_id, uintptr_t stack_addr)
{
    ap_state = AP_STATUS_INVALID;
    ap_pgd_addr = x86_get_cr3();
    ap_stack_addr = stack_addr;

    lapic_interrupt_full(0, apic_id, APIC_DELIVER_MODE_INIT, APIC_DEST_MODE_PHYSICAL, true, true, APIC_SHORTHAND_NONE);
    mdelay(100);
    lapic_interrupt_full(0, apic_id, APIC_DELIVER_MODE_INIT_DEASSERT, APIC_DEST_MODE_PHYSICAL, false, true, APIC_SHORTHAND_NONE);

    ap_state = AP_STATUS_BSP_STARTUP_SENT;
    pr_info2("smp: bsp sent startup to cpu %u", apic_id);

    mdelay(100);
    lapic_interrupt(X86_AP_TRAMPOLINE_ADDR >> 12, apic_id, APIC_DELIVER_MODE_STARTUP, APIC_DEST_MODE_PHYSICAL, APIC_SHORTHAND_NONE);
    mdelay(100);
    lapic_interrupt(X86_AP_TRAMPOLINE_ADDR >> 12, apic_id, APIC_DELIVER_MODE_STARTUP, APIC_DEST_MODE_PHYSICAL, APIC_SHORTHAND_NONE);

    wait_for(AP_STATUS_START_REQUEST);
    pr_info2("smp: cpu %u received start request", apic_id);

    ap_state = AP_STATUS_START;
    pr_info2("smp: started cpu %u", apic_id);
}

#undef wait_for

void x86_smp_init()
{
    pr_info("smp: boot cpu:");
    print_cpu_info();

    pr_info2("disabling 8259 PIC...");
    pic_disable();

    pr_info2("enabling APIC...");
    lapic_enable();

    u32 num_cpus = 0;
    processor_version_t info;
    cpuid_get_processor_info(&info);
    x86_platform.boot_cpu_id = info.ebx.local_apic_id;
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

    // !! TODO: is this the correct way?
    mos_startup_map_bytes(X86_AP_TRAMPOLINE_ADDR, X86_AP_TRAMPOLINE_ADDR, 4 KB, VM_WRITE);
    memcpy((void *) X86_AP_TRAMPOLINE_ADDR, (void *) (uintptr_t) &x86_ap_trampoline, 4 KB);

    for (u32 i = 0; i < num_cpus; i++)
    {
        u32 apic_id = lapics[i];
        if (apic_id == x86_platform.boot_cpu_id)
            continue;

        extern const void __MOS_KERNEL_HIGHER_STACK_TOP;
        const uintptr_t stack = (uintptr_t) &__MOS_KERNEL_HIGHER_STACK_TOP - (i * 1 MB);
        pr_info("smp: starting AP %d, LAPIC %d, stack " PTR_FMT, i, apic_id, stack);
        x86_cpu_start(apic_id, stack);
    }
}
