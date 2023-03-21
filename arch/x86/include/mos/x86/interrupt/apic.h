// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>

#define APIC_REG_LAPIC_ID 0x20
typedef enum
{
    APIC_DELIVER_MODE_NORMAL = 0,
    APIC_DELIVER_MODE_LOWEST_PRIORITY = 1,
    APIC_DELIVER_MODE_SMI = 2,
    APIC_DELIVER_MODE_NMI = 4,
    APIC_DELIVER_MODE_INIT = 5,
    APIC_DELIVER_MODE_INIT_DEASSERT = APIC_DELIVER_MODE_INIT,
    APIC_DELIVER_MODE_STARTUP = 6,
} lapic_delivery_mode_t;

typedef enum
{
    APIC_DEST_MODE_PHYSICAL = 0,
    APIC_DEST_MODE_LOGICAL = 1,
} lapic_dest_mode_t;

typedef enum
{
    APIC_SHORTHAND_NONE = 0,
    APIC_SHORTHAND_SELF = 1,
    APIC_SHORTHAND_ALL = 2,
    APIC_SHORTHAND_ALL_EXCLUDING_SELF = 3,
} lapic_shorthand_t;

void lapic_memory_setup(void);
void lapic_enable(void);
void lapic_interrupt(u8 vec, u8 dest, lapic_delivery_mode_t delivery_mode, lapic_dest_mode_t dest_mode, lapic_shorthand_t shorthand);
void lapic_interrupt_full(u8 vec, u8 dest, lapic_delivery_mode_t dliv_mode, lapic_dest_mode_t dstmode, bool lvl, bool trigger, lapic_shorthand_t sh);

u32 lapic_read32(u32 offset);
u64 lapic_read64(u32 offset);
void lapic_write32(u32 offset, u32 value);
void lapic_write64(u32 offset, u64 value);

void lapic_eoi(void);

should_inline u8 lapic_get_id(void)
{
    // https://stackoverflow.com/a/71756491
    // https://github.com/rust-osdev/apic/blob/master/src/registers.rs
    // shift 24 because the ID is in the upper 8 bits
    return lapic_read32(APIC_REG_LAPIC_ID) >> 24;
}

typedef enum
{
    IOAPIC_TRIGGER_MODE_EDGE = 0,
    IOAPIC_TRIGGER_MODE_LEVEL = 1,
} ioapic_trigger_mode_t;

typedef enum
{
    IOAPIC_POLARITY_ACTIVE_HIGH = 0,
    IOAPIC_POLARITY_ACTIVE_LOW = 1,
} ioapic_polarity_t;

void ioapic_init(void);
void ioapic_enable_with_mode(u32 irq, u32 cpu, ioapic_trigger_mode_t trigger_mode, ioapic_polarity_t polarity);
void ioapic_disable(u32 irq);

should_inline void ioapic_enable_interrupt(u32 irq, u32 cpu)
{
    ioapic_enable_with_mode(irq, cpu, IOAPIC_TRIGGER_MODE_EDGE, IOAPIC_POLARITY_ACTIVE_HIGH);
}
