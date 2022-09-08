// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/cpu/cpuid.h"

#include "mos/printk.h"

const char *cpuid_type_str[] = {
    [CPUID_T_OEM] = "OEM",
    [CPUID_T_IntelOverdrive] = "Intel Overdrive",
    [CPUID_T_DualProcessor] = "Dual Processor",
    [CPUID_T_Reserved] = "Reserved",
};

void x86_call_cpuid(u32 eax, x86_cpuid_info_t *cpuid)
{
    __asm__("cpuid" : "=a"(cpuid->eax), "=b"(cpuid->ebx), "=c"(cpuid->ecx), "=d"(cpuid->edx) : "0"(eax));
}

void cpuid_get_manufacturer(char *manufacturer)
{
    MOS_ASSERT(manufacturer != NULL);
    x86_cpuid_info_t cpuid = { 0 };
    x86_call_cpuid(0, &cpuid);
    *(u32 *) &manufacturer[0] = cpuid.ebx;
    *(u32 *) &manufacturer[4] = cpuid.edx;
    *(u32 *) &manufacturer[8] = cpuid.ecx;
    manufacturer[12] = 0;
}

void cpuid_get_processor_info(processor_version_t *info)
{
    x86_cpuid_info_t cpuid = { 0 };
    x86_call_cpuid(1, &cpuid);
    info->eax.raw = cpuid.eax;
    info->ebx.raw = cpuid.ebx;
    info->ecx.raw = cpuid.ecx;
    info->edx.raw = cpuid.edx;
}
