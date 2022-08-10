// SPDX-Licence-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef struct
{
    u32 eax;
    u32 ebx;
    u32 ecx;
    u32 edx;
} cpuid_t;

typedef enum
{
    CPUID_T_OEM,
    CPUID_T_IntelOverdrive,
    CPUID_T_DualProcessor,
    CPUID_T_Reserved
} cpuid_type_t;

extern const char *cpuid_type_str[];

typedef struct
{
    u8 stepping : 4;
    u8 model : 4;
    u8 family : 4;
    cpuid_type_t type : 2;
    u8 reserved1 : 2;
    u8 extended_model : 4;
    u8 extended_family : 8;
    u8 reserved2 : 4;
} __packed processor_version_t;

static_assert(sizeof(processor_version_t) == sizeof(u32), "processor_version is not 32 bits");

void x86_cpuid(u32 eax, cpuid_t *cpuid);
void x86_cpu_init();
