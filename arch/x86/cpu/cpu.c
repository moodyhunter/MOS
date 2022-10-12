// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/cpu/cpu.h"

#include "mos/x86/cpu/cpuid.h"

u32 x86_cpu_get_id(void)
{
    processor_version_t info;
    cpuid_get_processor_info(&info);
    return info.ebx.ebx.local_apic_id;
}
