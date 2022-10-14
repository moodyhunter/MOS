// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/interrupt/apic.h"

#include "mos/boot/startup.h"
#include "mos/constants.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/x86/acpi/acpi.h"
#include "mos/x86/cpu/cpu.h"
#include "mos/x86/cpu/cpuid.h"
#include "mos/x86/mm/paging.h"
#include "mos/x86/mm/paging_impl.h"

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

#define IA32_APIC_BASE_MSR        0x1B
#define IA32_APIC_BASE_MSR_ENABLE 0x800

void apic_set_base_addr(uintptr_t base_addr)
{
    u32 edx = 0;
    u32 eax = (base_addr & 0xfffff0000) | IA32_APIC_BASE_MSR_ENABLE;
    cpu_set_msr(IA32_APIC_BASE_MSR, eax, edx);
}

u32 apic_reg_read_offset_32(u32 offset)
{
    mos_debug("apic_reg_read_offset_32: offset: %x", offset);
#pragma message "TODO: this didn't work"
    return *(volatile u32 *) (lapic_paddr_base + offset);
}

void apic_reg_write_offset_32(u32 offset, u32 value)
{
    mos_debug("reg: 0x%x, value: 0x%.8x", offset, value);
    *(volatile u32 *) (lapic_paddr_base + offset) = value;
}

void apic_reg_write_offset_64(u32 offset, u64 value)
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
                         apic_shorthand_t shorthand)
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

void apic_interrupt(u8 vec, u8 dest, apic_delivery_mode_t delivery_mode, apic_dest_mode_t dest_mode, apic_shorthand_t shorthand)
{
    apic_interrupt_full(vec, dest, delivery_mode, dest_mode, true, false, shorthand);
}

void apic_enable()
{
    // CPUID.01h:EDX[bit 9]
    processor_version_t info;
    cpuid_get_processor_info(&info);

    if (!info.edx.edx.onboard_apic)
        mos_panic("APIC is not supported");

    if (!info.edx.edx.msr)
        mos_panic("MSR is not present");

    lapic_paddr_base = x86_acpi_madt->lapic_addr;
    pr_info("apic: mapped address: " PTR_FMT, lapic_paddr_base);

    apic_set_base_addr(lapic_paddr_base);

    // map both the current pagedir and x86_kpg_infra
    mos_startup_map_bios(lapic_paddr_base, 1 KB, VM_GLOBAL | VM_WRITE | VM_CACHE_DISABLED);
    pg_do_map_pages(x86_kpg_infra, BIOS_VADDR(lapic_paddr_base), lapic_paddr_base, 1, VM_GLOBAL | VM_WRITE | VM_CACHE_DISABLED);
    lapic_paddr_base = BIOS_VADDR(lapic_paddr_base);

#define APIC_SOFTWARE_ENABLE (1 << 8)
    // set the Spurious Interrupt Vector Register
    // To enable the APIC, set bit 8 (or 0x100) of this register.
    // All the other bits are currently reserved.
    apic_reg_write_offset_32(APIC_REG_SPURIOUS_INTR_VEC, apic_reg_read_offset_32(APIC_REG_SPURIOUS_INTR_VEC) | APIC_SOFTWARE_ENABLE);

    u32 version_reg = apic_reg_read_offset_32(APIC_REG_LAPIC_VERSION);
    u32 max_lvt_entry = (version_reg >> 16) & 0xff;
    u32 version_id = version_reg & 0xff;
    pr_info("apic: version: %x, max LVT entry: %x", version_id, max_lvt_entry);
}
