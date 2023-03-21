// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/boot/startup.h>
#include <mos/mm/paging/paging.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/x86/acpi/madt.h>
#include <mos/x86/cpu/smp.h>
#include <mos/x86/delays.h>
#include <mos/x86/interrupt/apic.h>
#include <mos/x86/mm/paging_impl.h>
#include <mos/x86/x86_platform.h>
#include <string.h>

volatile enum
{
    AP_STATUS_INVALID = 0,
    AP_STATUS_BSP_STARTUP_SENT = 1,
    AP_STATUS_START_REQUEST = 2,
    AP_STATUS_START = 3,
} ap_state;

#define X86_AP_TRAMPOLINE_ADDR 0x8000

extern char x86_ap_trampoline[];
extern x86_pgdir_entry startup_pgd[1024];
volatile uintptr_t ap_stack_addr = 0;
volatile const uintptr_t ap_pgd_addr = (uintptr_t) &startup_pgd;

// clang-format off
#define wait_for(state) do {} while (ap_state != state)
// clang-format on

static void start_ap(int apic_id)
{
    ap_state = AP_STATUS_INVALID;

    lapic_interrupt_full(0, apic_id, APIC_DELIVER_MODE_INIT, APIC_DEST_MODE_PHYSICAL, true, true, APIC_SHORTHAND_NONE);
    mdelay(50);
    lapic_interrupt_full(0, apic_id, APIC_DELIVER_MODE_INIT_DEASSERT, APIC_DEST_MODE_PHYSICAL, false, true, APIC_SHORTHAND_NONE);

    ap_state = AP_STATUS_BSP_STARTUP_SENT;
    mos_debug(x86_cpu, "bsp sent startup to cpu %u", apic_id);

    mdelay(50);
    lapic_interrupt(X86_AP_TRAMPOLINE_ADDR >> 12, apic_id, APIC_DELIVER_MODE_STARTUP, APIC_DEST_MODE_PHYSICAL, APIC_SHORTHAND_NONE);
    mdelay(50);
    lapic_interrupt(X86_AP_TRAMPOLINE_ADDR >> 12, apic_id, APIC_DELIVER_MODE_STARTUP, APIC_DEST_MODE_PHYSICAL, APIC_SHORTHAND_NONE);

    wait_for(AP_STATUS_START_REQUEST);
    mos_debug(x86_cpu, "cpu %u received start request", apic_id);

    ap_state = AP_STATUS_START;
    mos_debug(x86_cpu, "started cpu %u", apic_id);
}

#undef wait_for

void x86_smp_copy_trampoline(void)
{
    mos_startup_map_bytes(X86_AP_TRAMPOLINE_ADDR, X86_AP_TRAMPOLINE_ADDR, 4 KB, VM_READ | VM_WRITE | VM_EXEC);
    memcpy((void *) X86_AP_TRAMPOLINE_ADDR, x86_ap_trampoline, 4 KB);
}

void x86_smp_start_all(void)
{
    if (x86_platform.num_cpus == 1)
        return;

    pr_info("Starting APs...");

    extern const void __MOS_KERNEL_HIGHER_STACK_TOP;
    const size_t AP_STACKS_TOP = (uintptr_t) &__MOS_KERNEL_HIGHER_STACK_TOP - MOS_X86_INITIAL_STACK_SIZE; // minus the boot cpu stack
    for (u32 i = 0; i < x86_platform.num_cpus; i++)
    {
        const u32 apic_id = x86_cpu_lapic[i];
        if (apic_id == x86_platform.boot_cpu_id)
            continue;

        ap_stack_addr = AP_STACKS_TOP - i * MOS_X86_INITIAL_STACK_SIZE;
        pr_info("smp: starting AP %d, LAPIC %d, stack top: " PTR_FMT, i, apic_id, ap_stack_addr);
        start_ap(apic_id);
    }
}
