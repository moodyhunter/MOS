// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/assert.h"

#include <mos/syslog/printk.h>
#include <mos/x86/acpi/acpi.h>
#include <mos/x86/acpi/acpi_types.h>
#include <mos/x86/acpi/madt.h>
#include <mos/x86/x86_platform.h>

const acpi_madt_t *x86_acpi_madt = NULL;
ptr_t x86_ioapic_phyaddr = 0;
u32 x86_lapic_global_base = 0;

#define IOAPIC_IRQ_OVERRIDE_MAX 255 // u8 can hold up to 255
static u32 ioapic_irq_override[IOAPIC_IRQ_OVERRIDE_MAX] = { 0 };

u32 x86_ioapic_get_irq_override(u32 irq)
{
    if (irq >= IOAPIC_IRQ_OVERRIDE_MAX)
        return irq;
    return ioapic_irq_override[irq];
}

void madt_parse_table()
{
    if (!x86_acpi_madt)
        mos_panic("MADT not found");

    x86_platform.num_cpus = 0;

    for (u8 i = 0; i < IOAPIC_IRQ_OVERRIDE_MAX; i++)
        ioapic_irq_override[i] = i; // Default: no override

    madt_entry_foreach(entry, x86_acpi_madt)
    {
        switch (entry->type)
        {
            case 0:
            {
                acpi_madt_et0_lapic_t *lapic = container_of(entry, acpi_madt_et0_lapic_t, header);
                pr_dinfo2(x86_acpi, "MADT entry LAPIC [%p], id=%u, processor=%u, flags=0x%x", (void *) lapic, lapic->apic_id, lapic->processor_id, lapic->flags);

                bool enabled = lapic->flags & 1;        // bit 0: processor is enabled
                bool online_capable = lapic->flags & 2; // bit 1: online-capable

                // If flags bit 0 is set the CPU is able to be enabled, if it is not set you need to check bit 1. If that one is set you can still enable it, if it is not
                // the CPU can not be enabled and the OS should not try.

                if (!enabled && !online_capable)
                    continue;

                if (unlikely(x86_platform.num_cpus >= MOS_MAX_CPU_COUNT))
                    mos_panic("Too many CPUs");

                x86_platform.num_cpus++;
                break;
            }
            case 1:
            {
                acpi_madt_et1_ioapic_t *ioapic = container_of(entry, acpi_madt_et1_ioapic_t, header);
                pr_dinfo2(x86_acpi, "MADT entry IOAPIC [%p], id=%u, address=%x, global_irq_base=%u", (void *) ioapic, ioapic->id, ioapic->address,
                          ioapic->global_intr_base);
                if (unlikely(x86_ioapic_phyaddr))
                    mos_panic("Multiple IOAPICs not supported");
                x86_ioapic_phyaddr = ioapic->address;
                x86_lapic_global_base = ioapic->global_intr_base;
                break;
            }
            case 2:
            {
                acpi_madt_et2_ioapic_override_t *int_override = container_of(entry, acpi_madt_et2_ioapic_override_t, header);
                pr_dinfo2(x86_acpi, "MADT entry IOAPIC override [%p], bus=%u, flags=0x%x, 'irq source %u is now global irq %u'", (void *) int_override,
                          int_override->bus_source, int_override->flags, int_override->irq_source, int_override->global_intr);

                if (unlikely(int_override->bus_source != 0))
                    mos_panic("IOAPIC override for non-ISA bus not supported");

                if (unlikely(int_override->irq_source >= IOAPIC_IRQ_OVERRIDE_MAX))
                    mos_panic("IOAPIC override for IRQ >= 255 not supported");

                if (unlikely(ioapic_irq_override[int_override->irq_source] != int_override->irq_source))
                    mos_panic("Multiple IOAPIC overrides for the same IRQ not supported");

                ioapic_irq_override[int_override->irq_source] = int_override->global_intr;
                break;
            }
            case 3:
            {
                acpi_madt_et3_ioapic_nmi_t *int_override = container_of(entry, acpi_madt_et3_ioapic_nmi_t, header);
                pr_dinfo2(x86_acpi, "MADT entry IOAPIC NMI [%p], nmi_source=%u, global_irq=%u, flags=0x%x", (void *) int_override, int_override->nmi_source,
                          int_override->global_irq, int_override->flags);
#if MOS_DEBUG_FEATURE(x86_acpi)
                pr_warn("Unhandled MADT entry type 3 (IOAPIC NMI)");
#endif
                break;
            }
            case 4:
            {
                acpi_madt_et4_lapic_nmi_t *nmi = container_of(entry, acpi_madt_et4_lapic_nmi_t, header);
                pr_dinfo2(x86_acpi, "MADT entry LAPIC NMI [%p], processor=%u, flags=0x%x, lint=%u", (void *) nmi, nmi->processor_id, nmi->flags, nmi->lint_number);
#if MOS_DEBUG_FEATURE(x86_acpi)
                pr_warn("Unhandled MADT entry type 4 (LAPIC NMI)");
#endif
                break;
            }
            case 5:
            {
                acpi_madt_et5_lapic_addr_t *local_apic_nmi = container_of(entry, acpi_madt_et5_lapic_addr_t, header);
                pr_dinfo2(x86_acpi, "MADT entry LAPIC address override [%p], address=%llu", (void *) local_apic_nmi, local_apic_nmi->lapic_paddr);
#if MOS_DEBUG_FEATURE(x86_acpi)
                pr_warn("Unhandled MADT entry type 5 (LAPIC address override)");
#endif
                break;
            }
            case 9:
            {
                acpi_madt_et9_lx2apic_t *local_sapic_override = container_of(entry, acpi_madt_et9_lx2apic_t, header);
                pr_dinfo2(x86_acpi, "MADT entry local x2 SAPIC override [%p], x2apic_id=%u, flags=0x%x, acpi_id=%u", (void *) local_sapic_override,
                          local_sapic_override->processor_lx2apic_id, local_sapic_override->flags, local_sapic_override->acpi_id);
#if MOS_DEBUG_FEATURE(x86_acpi)
                pr_warn("Unhandled MADT entry type 9 (local x2 SAPIC override)");
#endif
                break;
            }
            default:
            {
                pr_warn("Strange MADT entry type %u", entry->type);
            }
        }
    }

    pr_dinfo2(x86_lapic, "platform has %u cpu(s)", x86_platform.num_cpus);
    return;
}
