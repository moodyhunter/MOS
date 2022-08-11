// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "mos/x86/cpu/cpuid.h"

#include "lib/containers.h"
#include "mos/printk.h"
#include "mos/types.h"
#include "mos/x86/acpi/acpi.h"
#include "mos/x86/acpi/acpi_types.h"
#include "mos/x86/x86_platform.h"

#include <cpuid.h>

const char *cpuid_type_str[] = {
    [CPUID_T_OEM] = "OEM",
    [CPUID_T_IntelOverdrive] = "Intel Overdrive",
    [CPUID_T_DualProcessor] = "Dual Processor",
    [CPUID_T_Reserved] = "Reserved",
};

void x86_call_cpuid(u32 eax, cpuid_t *cpuid)
{
    __asm__("cpuid" : "=a"(cpuid->eax), "=b"(cpuid->ebx), "=c"(cpuid->ecx), "=d"(cpuid->edx) : "0"(eax));
}

void cpuid_get_manufacturer(char *manufacturer)
{
    cpuid_t cpuid = { 0 };
    x86_call_cpuid(0, &cpuid);
    *(u32 *) &manufacturer[0] = cpuid.ebx;
    *(u32 *) &manufacturer[4] = cpuid.edx;
    *(u32 *) &manufacturer[8] = cpuid.ecx;
    manufacturer[12] = 0;
}

void cpuid_get_processor_info(processor_version_t *info)
{
    cpuid_t cpuid = { 0 };
    x86_call_cpuid(1, &cpuid);
    *(u32 *) &info->eax = cpuid.eax;
    *(u32 *) &info->ebx = cpuid.ebx;
    *(u32 *) &info->ecx = cpuid.ecx;
    *(u32 *) &info->edx = cpuid.edx;
}
