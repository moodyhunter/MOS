// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef struct
{
    u32 eax;
    u32 ebx;
    u32 ecx;
    u32 edx;
} x86_cpuid_info_t;

typedef enum
{
    CPUID_T_OEM,
    CPUID_T_IntelOverdrive,
    CPUID_T_DualProcessor,
    CPUID_T_Reserved
} cpuid_type_t;

typedef struct
{
    union
    {
        struct
        {
            u8 stepping : 4;
            u8 model : 4;
            u8 family : 4;
            cpuid_type_t type : 2;
            u8 reserved1 : 2;
            u8 ext_model : 4;
            u16 ext_family : 8;
            u8 reserved2 : 4;
        } __packed;
        reg32_t raw;
    } eax;

    union
    {
        struct
        {
            u8 brand_id : 8;
            u8 cpuflush_size : 8;
            /** Maximum number of addressable IDs for logical processors in this physical package;
             * The nearest power-of-2 integer that is not smaller than this value is the number of
             * unique initial APIC IDs reserved for addressing different logical processors in a
             * physical package.
             * Former use: Number of logical processors per physical processor; two for the Pentium
             * 4 processor with Hyper-Threading Technology.[10] */
            u8 logical_processors_per_package : 8;
            u8 local_apic_id : 8;
        } __packed;
        reg32_t raw;
    } ebx;

    union
    {
        struct
        {
            bool sse3 : 1;
            bool pclmulqdq : 1;
            bool dtes64 : 1;
            bool monitor_mwait : 1;
            bool dscpl : 1;
            bool vmx : 1;
            bool smx : 1;
            bool enhanced_speed_step : 1;

            bool thermal_monitor_2 : 1;
            bool supplemental_sse3 : 1;
            bool l1_context_id : 1;
            bool silicon_debug_interface : 1;
            bool fused_multiply_add : 1;
            bool cmpxchg16b : 1;
            bool can_disable_sending_task_priority_messages : 1;
            bool perfmon_and_debug : 1;

            bool _reserved1 : 1;
            bool process_ctx_id : 1;
            bool direct_cache_access_for_dma : 1;
            bool sse41 : 1;
            bool sse42 : 1;
            bool x2apic : 1;
            bool movbe : 1;
            bool popcnt : 1;

            bool tsc_deadline : 1;
            bool aes : 1;
            bool xsave : 1;
            bool osxsave : 1;
            bool avx : 1;
            bool f16c : 1;
            bool rdrand : 1;
            bool hypervisor : 1;
        } __packed;
        reg32_t raw;
    } ecx;

    union
    {
        struct
        {
            bool x87_fpu : 1;
            bool v8086_ext : 1;
            bool debugging_extensions : 1;
            bool page_size_extensions : 1;
            bool time_stamp_counter : 1;
            bool msr : 1;
            bool physical_address_extensions : 1;
            bool machine_check_exception : 1;

            bool cmpxchg8b_supported : 1;
            bool onboard_apic : 1;
            bool _reserved : 1;
            bool sysenter_sysexit : 1;
            bool memory_type_range_registers : 1;
            bool page_global_enable : 1;
            bool machine_check_arch : 1;
            bool fcmov_supported : 1;

            bool page_attribute_table : 1;
            bool pse_36 : 1;
            bool processor_sn : 1;
            bool clflush_instruction : 1;
            bool _reserved_2 : 1;
            bool debug_store : 1;
            bool thermal_msrs_for_acpi : 1;
            bool mmx_extensions : 1;

            bool fxsave_fxstor_supported : 1;
            bool sse_supported : 1;
            bool sse2_supported : 1;
            bool cpu_cache_impl_self_snoop : 1;
            bool hyper_threading : 1;
            bool thermal_monitor_temp_limits : 1;
            bool ia64_emulating_x86 : 1;
            bool pending_break_enable : 1;
        } __packed;
        reg32_t raw;
    } edx;
} __packed processor_version_t;

static_assert(sizeof(processor_version_t) == sizeof(u32) * 4, "processor_version is not 32 bits");

typedef struct
{
    u8 brand_id;
    u8 reserved1;
    u16 reserved2;
    u32 reserved3;
} __packed processor_brand_t;

void x86_call_cpuid(u32 eax, x86_cpuid_info_t *cpuid);
void cpuid_get_manufacturer(char *manufacturer);
void cpuid_get_processor_info(processor_version_t *info);
void cpuid_get_brand_string(char *brand_string);
void cpuid_print_cpu_info(void);
