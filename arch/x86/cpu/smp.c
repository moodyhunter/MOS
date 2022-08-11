// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "mos/x86/cpu/smp.h"

#include "mos/printk.h"
#include "mos/x86/acpi/acpi.h"
#include "mos/x86/acpi/acpi_types.h"
#include "mos/x86/cpu/cpuid.h"
#include "mos/x86/x86_platform.h"

void x86_smp_init()
{
    x86_cpu_info.cpu_count = 0;
    madt_entry_foreach(entry, x86_acpi_madt)
    {
        if (entry->type == 0)
            x86_cpu_info.cpu_count++;
    }

    pr_info("CPUs: %u", x86_cpu_info.cpu_count);

    char manufacturer[13];
    cpuid_get_manufacturer(manufacturer);
    processor_version_t info;
    cpuid_get_processor_info(&info);
    pr_info("cpu: '%s': Family %u, Model %u, Stepping %u", manufacturer, info.eax.family, info.eax.model, info.eax.stepping);
    pr_info("cpu: %-10s: %s", "type", cpuid_type_str[info.eax.type]);
    pr_info("cpu: %-10s: %u", "ext model", info.eax.extended_model);
    pr_info("cpu: %-10s: %u", "ext family", info.eax.extended_family);
}
