// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

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

typedef enum
{
    APIC_DELIVER_MODE_FIXED = 0,
    APIC_DELIVER_MODE_LOWEST_PRIORITY = 1,
    APIC_DELIVER_MODE_SMI = 2,
    APIC_DELIVER_MODE_NMI = 4,
    APIC_DELIVER_MODE_INIT = 5,
    APIC_DELIVER_MODE_INIT_DEASSERT = APIC_DELIVER_MODE_INIT,
    APIC_DELIVER_MODE_STARTUP = 6,
} apic_delivery_mode_t;

typedef enum
{
    APIC_DEST_MODE_PHYSICAL = 0,
    APIC_DEST_MODE_LOGICAL = 1,
} apic_dest_mode_t;

typedef enum
{
    APIC_SHORTHAND_NONE = 0,
    APIC_SHORTHAND_SELF = 1,
    APIC_SHORTHAND_ALL = 2,
    APIC_SHORTHAND_ALL_EXCLUDING_SELF = 3,
} apic_shorthand_t;

void apic_assert_supported();
void apic_enable();
void apic_interrupt(u8 vec, u8 dest, apic_delivery_mode_t delivery_mode, apic_dest_mode_t dest_mode, apic_shorthand_t shorthand);
void apic_interrupt_full(u8 vec, u8 dest, apic_delivery_mode_t dliv_mode, apic_dest_mode_t dstmode, bool lvl, bool trigger, apic_shorthand_t sh);

u32 apic_reg_read_offset_32(u32 offset);
void apic_reg_write_offset_32(u32 offset, u32 value);
void apic_reg_write_offset_64(u32 offset, u64 value);
