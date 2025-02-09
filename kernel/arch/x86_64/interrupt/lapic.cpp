// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/mm/paging/paging.hpp>
#include <mos/mm/physical/pmm.hpp>
#include <mos/mos_global.h>
#include <mos/platform/platform.hpp>
#include <mos/syslog/printk.hpp>
#include <mos/x86/acpi/madt.hpp>
#include <mos/x86/cpu/cpu.hpp>
#include <mos/x86/cpu/cpuid.hpp>
#include <mos/x86/interrupt/apic.hpp>
#include <mos/x86/mm/paging_impl.hpp>
#include <mos/x86/x86_platform.hpp>

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

#define IA32_APIC_BASE_MSR 0x1B

static ptr_t lapic_regs = 0;

u32 lapic_read32(u32 offset)
{
    MOS_ASSERT(lapic_regs);
    pr_dinfo2(x86_lapic, "reading reg: %x, ptr: " PTR_FMT, offset, lapic_regs + offset);
    return *(volatile u32 *) (lapic_regs + offset);
}

u64 lapic_read64(u32 offset)
{
    MOS_ASSERT(lapic_regs);
    pr_dinfo2(x86_lapic, "reading reg: %x, ptr: " PTR_FMT, offset, lapic_regs + offset);
    const u32 high = *(volatile u32 *) (lapic_regs + offset + 0x10);
    const u32 low = *(volatile u32 *) (lapic_regs + offset);
    return ((u64) high << 32) | low;
}

void lapic_write32(u32 offset, u32 value)
{
    MOS_ASSERT(lapic_regs);
    pr_dinfo2(x86_lapic, "writing reg: %x, value: 0x%.8x, ptr: " PTR_FMT, offset, value, lapic_regs + offset);
    *(volatile u32 *) (lapic_regs + offset) = value;
}

void lapic_write64(u32 offset, u64 value)
{
    MOS_ASSERT(lapic_regs);
    pr_dinfo2(x86_lapic, "writing reg: %x, value: 0x%.16llx, ptr: " PTR_FMT, offset, value, lapic_regs + offset);
    *(volatile u32 *) (lapic_regs + offset + 0x10) = value >> 32;
    *(volatile u32 *) (lapic_regs + offset) = value;
}

static void lapic_wait_sent(void)
{
    // Wait for the delivery status bit to be set
    while (lapic_read32(APIC_INTERRUPT_COMMAND_REG_BEGIN) & BIT(12))
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

static void lapic_memory_setup(void)
{
    if (!cpu_has_feature(CPU_FEATURE_APIC))
        mos_panic("APIC is not supported");

    if (!cpu_has_feature(CPU_FEATURE_MSR))
        mos_panic("MSR is not supported");

    const ptr_t base_addr = x86_acpi_madt->lapic_addr;
    pr_dinfo2(x86_lapic, "base address: " PTR_FMT, base_addr);

    if (!pmm_find_reserved_region(base_addr))
    {
        pr_info("LAPIC: reserve " PTR_FMT, base_addr);
        pmm_reserve_address(base_addr);
    }

    lapic_regs = pa_va(base_addr);
}

void lapic_enable(void)
{
    if (once())
        lapic_memory_setup();

    // (https://wiki.osdev.org/APIC#Local_APIC_configuration)
    // To enable the Local APIC to receive interrupts it is necessary to configure the "Spurious Interrupt Vector Register".
    // The correct value for this field is
    // - the IRQ number that you want to map the spurious interrupts to within the lowest 8 bits, and
    // - the 8th bit set to 1
    // to actually enable the APIC
    const u32 spurious_intr_vec = lapic_read32(APIC_REG_SPURIOUS_INTR_VEC) | 0x100;
    lapic_write32(APIC_REG_SPURIOUS_INTR_VEC, spurious_intr_vec);
}

void lapic_set_timer(u32 initial_count)
{
    lapic_write32(APIC_REG_TIMER_DIVIDE_CONFIG, 0x3);           // divide by 16
    lapic_write32(APIC_REG_LVT_TIMER, 0x20000 | 32);            // periodic timer mode
    lapic_write32(APIC_REG_TIMER_INITIAL_COUNT, initial_count); // start the timer
}

void lapic_eoi(void)
{
    lapic_write32(APIC_REG_EOI, 0);
}
