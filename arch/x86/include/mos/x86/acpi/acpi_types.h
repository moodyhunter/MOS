// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef struct
{
    char signature[8];
    u8 checksum;
    char oem_id[6];
    u8 revision;
    u32 rsdt_addr;
} __packed acpi_rsdp_v1_t;

static_assert(sizeof(acpi_rsdp_v1_t) == 20, "acpi_rsdp_v1_t is not 20 bytes");

typedef struct
{
    acpi_rsdp_v1_t v1;
    u32 length;
    u64 xsdt_addr;
    u8 checksum;
    u8 reserved[3];
} __packed acpi_rsdp_v2_t;

#define ACPI_SIGNATURE_RSDP "RSD PTR "
typedef acpi_rsdp_v2_t acpi_rsdp_t;

typedef struct
{
    char signature[4];
    u32 length;
    u8 revision;
    u8 checksum;
    char oem_id[6];
    char oem_table_id[8];
    u32 oem_revision;
    u32 creator_id;
    u32 creator_revision;
} acpi_sdt_header_t;

typedef struct
{
    acpi_sdt_header_t sdt_header;
    acpi_sdt_header_t *sdts[];
} acpi_rsdt_t;

#define ACPI_SIGNATURE_RSDT "RSDT" // Root System Description Table

typedef struct
{
    u8 addr_space;
    u8 bit_width;
    u8 bit_offset;
    u8 access_size;
    u64 paddr;
} generic_addr_t;

typedef struct
{
    acpi_sdt_header_t sdt_header;
    u32 fw_control;
    u32 dsdt;

    // field used in ACPI 1.0; no longer in use, for compatibility only
    u8 reserved;

    u8 preferred_power_management_profile;
    u16 sci_interrupt;
    u32 smi_command_port;
    u8 acpi_enable;
    u8 acpi_disable;
    u8 s4_bios_req;
    u8 pstate_control;
    u32 pm1a_event_block;
    u32 pm1b_event_block;
    u32 pm1a_control_block;
    u32 pm1b_control_block;
    u32 pm2_control_block;
    u32 pm_timer_block;
    u32 gpe0_block;
    u32 gpe1_block;
    u8 pm1_event_len;
    u8 pm1_control_len;
    u8 pm2_control_len;
    u8 pm_timer_len;
    u8 gpe0_len;
    u8 gpe1_len;
    u8 gpe1_base;
    u8 c_state_control;
    u16 worst_c2_latency;
    u16 worst_c3_latency;
    u16 flush_size;
    u16 flush_stride;
    u8 duty_offset;
    u8 duty_width;
    u8 day_alarm;
    u8 month_alarm;
    u8 century;

    // reserved in ACPI 1.0; used since ACPI 2.0+
    u16 boot_arch_flags;

    u8 reserved2;
    u32 flags;

    // 12 byte structure; see below for details
    generic_addr_t reset_reg;

    u8 reset_value;
    u8 reserved3[3];

    // 64bit pointers - Available on ACPI 2.0+
    u64 X_FirmwareControl;
    u64 X_Dsdt;

    generic_addr_t X_PM1aEventBlock;
    generic_addr_t X_PM1bEventBlock;
    generic_addr_t X_PM1aControlBlock;
    generic_addr_t X_PM1bControlBlock;
    generic_addr_t X_PM2ControlBlock;
    generic_addr_t X_PMTimerBlock;
    generic_addr_t X_GPE0Block;
    generic_addr_t X_GPE1Block;
} acpi_fadt_t;

#define ACPI_SIGNATURE_FADT "FACP" // Fixed ACPI Description Table

typedef struct
{
    u8 type;
    u8 record_length;
} acpi_madt_entry_header_t;

// Type 0 - Processor Local APIC
typedef struct
{
    acpi_madt_entry_header_t madt_entry_header;
    u8 processor_id;
    u8 apic_id;
    u32 flags;
} acpi_madt_et0_lapic_t;

// Type 1 - I/O APIC
typedef struct
{
    acpi_madt_entry_header_t madt_entry_header;
    u8 io_apic_id;
    u8 reserved;
    u32 address;
    u32 global_system_interrupt_base;
} acpi_madt_et1_ioapic_t;

// Type 2 - IO/APIC Interrupt Source Override
typedef struct
{
    acpi_madt_entry_header_t madt_entry_header;
    u8 bus_source;
    u8 irq_source;
    u32 global_system_interrupt;
    u16 flags;
} acpi_madt_et2_ioapic_override_t;

// Type 3 - IO/APIC Non-maskable interrupt source
typedef struct
{
    acpi_madt_entry_header_t madt_entry_header;
    u8 nmi_source;
    u8 reserved;
    u16 flags;
    u32 global_system_interrupt;
} acpi_madt_et3_ioapic_nmi_t;

// Type 4 - Local APIC Non-maskable interrupts
typedef struct
{
    acpi_madt_entry_header_t madt_entry_header;
    u8 acpi_processor_id;
    u16 flags;
    u8 lint_number;
} acpi_madt_et4_lapic_nmi_t;

// Type 5 - Local APIC Address Override
typedef struct
{
    acpi_madt_entry_header_t madt_entry_header;
    u16 reserved;
    u64 lapic_paddr;
} acpi_madt_et5_lapic_addr_t;

// Type 9 - Processor Local x2APIC
typedef struct
{
    acpi_madt_entry_header_t madt_entry_header;
    u16 reserved;
    u32 processor_lx2apic_id;
    u32 flags;
    u32 acpi_id;
} acpi_madt_et9_lx2apic_t;

typedef struct
{
    acpi_sdt_header_t sdt_header;
    u32 lapic_addr;
    u32 flags;
} acpi_madt_t;

#define ACPI_SIGNATURE_MADT "APIC" // Multiple APIC Description Table

#define madt_is_valid_entry_type(type) ((type >= 0 && type <= 5) || type == 9)
#define madt_entry_foreach(var, madt)                                                                                                           \
    for (acpi_madt_entry_header_t *var = (acpi_madt_entry_header_t *) ((char *) madt + sizeof(acpi_madt_t));                                    \
         madt_is_valid_entry_type(var->type); var = (acpi_madt_entry_header_t *) ((char *) var + entry->record_length))

typedef struct
{
    acpi_sdt_header_t sdt_header;
    u8 hardware_rev_id;
    u8 compatible_id : 5;
    u8 counter_size : 1;
    u8 reserved : 1;
    u8 legacy_replacement : 1;
    u16 pci_vendor_id;
    generic_addr_t addr;
    u8 hpet_number;
    u16 minimum_tick;
    u8 page_protection;
} __packed acpi_hpet_t;

#define ACPI_SIGNATURE_HPET "HPET" // High Precision Event Timer Description Table

typedef struct
{
    acpi_sdt_header_t header;
    u8 *definition_block;
    bool valid;
} s_dsdt;

acpi_rsdp_t *find_acpi_rsdp(uintptr_t start, size_t size);
