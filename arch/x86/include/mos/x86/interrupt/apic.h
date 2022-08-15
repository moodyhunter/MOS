// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

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
} apic_dest_shorthand_t;

void apic_assert_supported();
void apic_enable();

void apic_interrupt_full(u8 vec, u8 dest, apic_delivery_mode_t delivery_mode, apic_dest_mode_t dest_mode, bool level, bool trigger,
                         apic_dest_shorthand_t shorthand);
void apic_interrupt(u8 vec, u8 dest, apic_delivery_mode_t delivery_mode, apic_dest_mode_t dest_mode, apic_dest_shorthand_t shorthand);
