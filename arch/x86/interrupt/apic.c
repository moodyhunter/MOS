// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/interrupt/apic.h"

#include "mos/mos_global.h"
#include "mos/printk.h"
#include "mos/x86/acpi/acpi.h"
#include "mos/x86/cpu/cpu.h"
#include "mos/x86/cpu/cpuid.h"

#define APIC_REG_LAPIC_ID            0x20
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

static uintptr_t lapic_paddr_base = 0;

void apic_assert_supported()
{
    // CPUID.01h:EDX[bit 9]
    processor_version_t info;
    cpuid_get_processor_info(&info);

    if (!info.edx.onboard_apic)
        mos_panic("APIC is not supported");

    if (!info.edx.msr)
        mos_panic("MSR is not present");
}

#define IA32_APIC_BASE_MSR        0x1B
#define IA32_APIC_BASE_MSR_ENABLE 0x800

void apic_set_base_addr(uintptr_t base_addr)
{
    u32 edx = 0;
    u32 eax = (base_addr & 0xfffff0000) | IA32_APIC_BASE_MSR_ENABLE;
    cpu_set_msr(IA32_APIC_BASE_MSR, eax, edx);
}

static u32 apic_reg_read_offset_32(u32 offset)
{
    return *(volatile u32 *) (lapic_paddr_base + offset);
}

static void apic_reg_write_offset_32(u32 offset, u32 value)
{
    mos_debug("reg: 0x%x, value: 0x%.8x", offset, value);
    *(volatile u32 *) (lapic_paddr_base + offset) = value;
}

static void apic_reg_write_offset_64(u32 offset, u64 value)
{
    mos_debug("reg: 0x%x, value: 0x%.16llx", offset, value);
    *(volatile u32 *) (lapic_paddr_base + offset + 0x10) = value >> 32;
    *(volatile u32 *) (lapic_paddr_base + offset) = value & 0xffffffff;
}

static void apic_wait_sent()
{
    // Wait for the delivery status bit to be set
    while (apic_reg_read_offset_32(APIC_INTERRUPT_COMMAND_REG_BEGIN) & SET_BITS(12, 1, 1))
        ;
}

void apic_interrupt_full(u8 vec, u8 dest, apic_delivery_mode_t delivery_mode, apic_dest_mode_t dest_mode, bool level, bool trigger,
                         apic_dest_shorthand_t shorthand)
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

    apic_reg_write_offset_32(APIC_REG_ERROR_STATUS, 0);
    apic_reg_write_offset_64(APIC_INTERRUPT_COMMAND_REG_BEGIN, value);
    apic_wait_sent();
}

void apic_interrupt(u8 vec, u8 dest, apic_delivery_mode_t delivery_mode, apic_dest_mode_t dest_mode, apic_dest_shorthand_t shorthand)
{
    apic_interrupt_full(vec, dest, delivery_mode, dest_mode, true, false, shorthand);
}

void apic_enable()
{
    apic_assert_supported();
    lapic_paddr_base = x86_acpi_madt->lapic_addr;
    pr_info("apic: mapped address: " PTR_FMT, lapic_paddr_base);
    apic_set_base_addr(lapic_paddr_base);

#define APIC_SOFTWARE_ENABLE 1 << 8
    // set the Spurious Interrupt Vector Register
    // To enable the APIC, set bit 8 (or 0x100) of this register.
    // All the other bits are currently reserved.
    apic_reg_write_offset_32(APIC_REG_SPURIOUS_INTR_VEC, apic_reg_read_offset_32(APIC_REG_SPURIOUS_INTR_VEC) | APIC_SOFTWARE_ENABLE);

    u32 version_reg = apic_reg_read_offset_32(APIC_REG_LAPIC_VERSION);
    u32 max_lvt_entry = (version_reg >> 16) & 0xff;
    u32 version_id = version_reg & 0xff;
    pr_info("apic: version: %x, max LVT entry: %x", version_id, max_lvt_entry);
}
