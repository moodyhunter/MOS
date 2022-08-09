// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "mos/x86/cpu/cpuid.h"

#include "mos/printk.h"
#include "mos/types.h"

#include <cpuid.h>

const char *cpuid_type_str[] = {
    [CPUID_T_OEM] = "OEM",
    [CPUID_T_IntelOverdrive] = "Intel Overdrive",
    [CPUID_T_DualProcessor] = "Dual Processor",
    [CPUID_T_Reserved] = "Reserved",
};

void x86_cpuid(u32 eax, cpuid_t *cpuid)
{
    __asm__("cpuid" : "=a"(cpuid->eax), "=b"(cpuid->ebx), "=c"(cpuid->ecx), "=d"(cpuid->edx) : "0"(eax));
}

void cpuid_manufacturer(char *manufacturer)
{
    cpuid_t cpuid = { 0 };
    x86_cpuid(0, &cpuid);
    *(u32 *) &manufacturer[0] = cpuid.ebx;
    *(u32 *) &manufacturer[4] = cpuid.edx;
    *(u32 *) &manufacturer[8] = cpuid.ecx;
    manufacturer[12] = 0;
}

void cpuid_processor_info(processor_version_t *info)
{
    cpuid_t cpuid = { 0 };
    x86_cpuid(1, &cpuid);
    info->stepping = cpuid.eax & 0xf;
    info->model = (cpuid.eax >> 4) & 0xf;
    info->family = (cpuid.eax >> 8) & 0xf;
    info->type = (cpuid.eax >> 12) & 0x3;
    info->extended_model = (cpuid.eax >> 16) & 0xf;
    info->extended_family = (cpuid.eax >> 20) & 0xff;
}

void x86_cpuid_dump()
{
    char manufacturer[13];
    cpuid_manufacturer(manufacturer);
    processor_version_t info;
    cpuid_processor_info(&info);
    pr_info("cpu: '%s': Family %u, Model %u, Stepping %u", manufacturer, info.family, info.model, info.stepping);
    pr_info("cpu: %-10s: %s", "type", cpuid_type_str[info.type]);
    pr_info("cpu: %-10s: %u", "ext model", info.extended_model);
    pr_info("cpu: %-10s: %u", "ext family", info.extended_family);
}
