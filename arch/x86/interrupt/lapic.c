// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/paging.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/x86/acpi/madt.h"
#include "mos/x86/cpu/cpu.h"
#include "mos/x86/cpu/cpuid.h"
#include "mos/x86/interrupt/apic.h"
#include "mos/x86/mm/paging.h"
#include "mos/x86/mm/paging_impl.h"
#include "mos/x86/x86_platform.h"

#define APIC_REG_LAPIC_VERSION       0x30
#define APIC_REG_PRIO_TASK           0x80
#define APIC_REG_PRIO_ARBITRATION    0x90
#define APIC_REG_PRIO_PROCESSOR      0xA0
#define APIC_REG_EOI                 0xB0
#define APIC_REG_REMOTE_READ         0xC0
#define APIC_REG_LOGICAL_DEST        0xD0
#define APIC_REG_DEST_FORMAT         0xE0
#define APIC_REG_SPURIOUS_INTR_VEC   0xF0
#define APIC_REG_ERROR_STATUS        0x280
#define APIC_REG_TIMER_INITIAL_COUNT 0x380
#define APIC_REG_TIMER_CURRENT_COUNT 0x390
#define APIC_REG_TIMER_DIVIDE_CONFIG 0x3E0

#define APIC_REG_LVT_CMCI_INTR      0x2F0
#define APIC_REG_LVT_TIMER          0x320
#define APIC_REG_LVT_THERMAL_SENSOR 0x330
#define APIC_REG_LVT_PERF_MON_CTR   0x340
#define APIC_REG_LVT_LINT0          0x350
#define APIC_REG_LVT_LINT1          0x360
#define APIC_REG_LVT_ERROR          0x370

#define APIC_IN_SERVICE_REG_BEGIN 0x100
#define APIC_IN_SERVICE_REG_END   0x170

#define APIC_TRIGGER_MODE_REG_BEGIN 0x180
#define APIC_TRIGGER_MODE_REG_END   0x1F0

#define APIC_TRIGGER_MODE_REG_TMR_BEGIN 0x180
#define APIC_TRIGGER_MODE_REG_TMR_END   0x1F0

#define APIC_INTERRUPT_REQUEST_REG_BEGIN 0x200
#define APIC_INTERRUPT_REQUEST_REG_END   0x270

#define APIC_INTERRUPT_COMMAND_REG_BEGIN 0x300
#define APIC_INTERRUPT_COMMAND_REG_END   0x310

static volatile u32 *lapic_regs = NULL;

#define IA32_APIC_BASE_MSR        0x1B
#define IA32_APIC_BASE_MSR_ENABLE 0x800

void lapic_set_base_addr(uintptr_t base_addr)
{
    u32 edx = 0;
    u32 eax = (base_addr & 0xfffff0000) | IA32_APIC_BASE_MSR_ENABLE;
    cpu_set_msr(IA32_APIC_BASE_MSR, eax, edx);
}

u32 lapic_read32(u32 offset)
{
    mos_debug(x86_lapic, "reg: %x", offset);
    return lapic_regs[offset / sizeof(u32)];
}

u64 lapic_read64(u32 offset)
{
    mos_debug(x86_lapic, "reg: %x", offset);
    u32 high = lapic_regs[(offset + 0x10) / sizeof(u32)];
    u32 low = lapic_regs[offset / sizeof(u32)];
    return ((u64) high << 32) | low;
}

void lapic_write32(u32 offset, u32 value)
{
    mos_debug(x86_lapic, "reg: %x, value: 0x%.8x", offset, value);
    lapic_regs[offset / sizeof(u32)] = value;
#if MOS_DEBUG_FEATURE(x86_lapic)
    u32 read_value = lapic_read32(offset);
    if (read_value != value)
        mos_warn("INCORRECT: 0x%.8x", read_value);
#endif
}

void lapic_write64(u32 offset, u64 value)
{
    mos_debug(x86_lapic, "reg: %x, value: 0x%.16llx", offset, value);
    lapic_regs[(offset + 0x10) / sizeof(u32)] = value >> 32;
    lapic_regs[offset / sizeof(u32)] = value & 0xffffffff;
#if MOS_DEBUG_FEATURE(x86_lapic)
    u64 read_value = lapic_read64(offset);
    if (read_value != value)
        mos_warn("INCORRECT: 0x%.16llx", read_value);
#endif
}

static void lapic_wait_sent(void)
{
    // Wait for the delivery status bit to be set
    while (lapic_read32(APIC_INTERRUPT_COMMAND_REG_BEGIN) & SET_BITS(12, 1, 1))
        ;
}

void lapic_interrupt_full(u8 vec, u8 dest, lapic_delivery_mode_t delivery_mode, lapic_dest_mode_t dest_mode, bool level, bool trigger, lapic_shorthand_t shorthand)
{
    u64 value = 0;
    value |= SET_BITS(0, 8, vec);           // Interrupt Vector
    value |= SET_BITS(8, 3, delivery_mode); // Delivery mode
    value |= SET_BITS(11, 1, dest_mode);    // Logical destination mode
    value |= SET_BITS(12, 1, 0);            // Delivery status (0)
    value |= SET_BITS(14, 1, level);        // Level
    value |= SET_BITS(15, 1, trigger);      // Trigger mode
    value |= SET_BITS(18, 2, shorthand);    // Destination shorthand
    value |= SET_BITS(56, 8, (u64) dest);   // Destination

    lapic_write32(APIC_REG_ERROR_STATUS, 0);
    lapic_write64(APIC_INTERRUPT_COMMAND_REG_BEGIN, value);
    lapic_wait_sent();
}

void lapic_interrupt(u8 vec, u8 dest, lapic_delivery_mode_t delivery_mode, lapic_dest_mode_t dest_mode, lapic_shorthand_t shorthand)
{
    lapic_interrupt_full(vec, dest, delivery_mode, dest_mode, true, false, shorthand);
}

void lapic_memory_setup(void)
{
    // CPUID.01h:EDX[bit 9]
    processor_version_t info;
    cpuid_get_processor_info(&info);

    if (!info.edx.onboard_apic)
        mos_panic("APIC is not supported");

    if (!info.edx.msr)
        mos_panic("MSR is not present");

    uintptr_t base_addr = x86_acpi_madt->lapic_addr;
    pr_info("LAPIC: base address: " PTR_FMT, base_addr);

    if ((uintptr_t) base_addr != BIOS_VADDR(base_addr))
    {
        pr_info("LAPIC: remapping it to " PTR_FMT, BIOS_VADDR(base_addr));
        base_addr = BIOS_VADDR(base_addr);
    }

    mm_map_pages(x86_platform.kernel_pgd, base_addr, base_addr, 1, VM_RW);
    lapic_regs = (u32 *) base_addr;
}

void lapic_enable(void)
{
    lapic_set_base_addr((uintptr_t) lapic_regs);

    // (https://wiki.osdev.org/APIC#Local_APIC_configuration)
    // To enable the Local APIC to receive interrupts it is necessary to configure the "Spurious Interrupt Vector Register".
    // The correct value for this field is
    // - the IRQ number that you want to map the spurious interrupts to within the lowest 8 bits, and
    // - the 8th bit set to 1
    // to actually enable the APIC
    lapic_write32(APIC_REG_SPURIOUS_INTR_VEC, lapic_read32(APIC_REG_SPURIOUS_INTR_VEC) | (1 << 8));

    u32 current_cpu_id = lapic_get_id();
    u32 version_reg = lapic_read32(APIC_REG_LAPIC_VERSION);
    u32 max_lvt_entry = (version_reg >> 16) & 0xff;
    u32 version_id = version_reg & 0xff;
    pr_info("LAPIC{%d}: version: %x, max LVT entry: %x", current_cpu_id, version_id, max_lvt_entry);
}

void lapic_eoi(void)
{
    lapic_write32(APIC_REG_EOI, 0);
}
