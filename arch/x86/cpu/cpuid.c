// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/cpu/cpuid.h"

#include "mos/printk.h"

const char *const cpuid_type_str[] = {
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

void cpuid_get_brand_string(char *brand_string)
{
    x86_cpuid_info_t cpuid = { 0 };
    x86_call_cpuid(0x80000002, &cpuid);
    *(u32 *) &brand_string[0] = cpuid.eax;
    *(u32 *) &brand_string[4] = cpuid.ebx;
    *(u32 *) &brand_string[8] = cpuid.ecx;
    *(u32 *) &brand_string[12] = cpuid.edx;
    x86_call_cpuid(0x80000003, &cpuid);
    *(u32 *) &brand_string[16] = cpuid.eax;
    *(u32 *) &brand_string[20] = cpuid.ebx;
    *(u32 *) &brand_string[24] = cpuid.ecx;
    *(u32 *) &brand_string[28] = cpuid.edx;
    x86_call_cpuid(0x80000004, &cpuid);
    *(u32 *) &brand_string[32] = cpuid.eax;
    *(u32 *) &brand_string[36] = cpuid.ebx;
    *(u32 *) &brand_string[40] = cpuid.ecx;
    *(u32 *) &brand_string[44] = cpuid.edx;
    brand_string[48] = 0;
}

void cpuid_print_cpu_info()
{
    char manufacturer[13];
    cpuid_get_manufacturer(manufacturer);
    processor_version_t info;
    cpuid_get_processor_info(&info);
    char brand_string[49];
    cpuid_get_brand_string(brand_string);
    pr_info2("CPU: %s (%s)", brand_string, manufacturer);
    pr_info2("  Family %u, Model %u, Stepping %u", info.eax.family, info.eax.model, info.eax.stepping);
    pr_info2("  Type: %s, Ext family: %u, Ext model: %u", cpuid_type_str[info.eax.type], info.eax.ext_family, info.eax.ext_model);
}
