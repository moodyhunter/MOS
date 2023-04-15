// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/types.h>
#include <mos/x86/devices/port.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    u8 base_class;
    u8 sub_class;
    u8 prog_if;
    const char *name;
} pci_class_t;

const pci_class_t known_classes[] = {
#define _X(x, y, z, name) { 0x##x, 0x##y, 0x##z, name },
#include "pci_classes.h"
#undef _X
};

static const char *get_known_class_name(u8 base_class, u8 sub_class, u8 prog_if)
{
    for (size_t i = 0; i < MOS_ARRAY_SIZE(known_classes); i++)
    {
        const u8 k_baseclass = known_classes[i].base_class;
        const u8 k_subclass = known_classes[i].sub_class;
        const u8 k_progif = known_classes[i].prog_if;

        if (k_baseclass == base_class && k_subclass == sub_class && k_progif == prog_if)
            return strdup(known_classes[i].name);
    }

    char *name = malloc(64);
    sprintf(name, "Unknown class: %02x:%02x:%02x", base_class, sub_class, prog_if);
    return name;
}

static u8 pci_read8(u8 bus, u8 slot, u8 func, u8 offset)
{
    const u32 address = (u32) ((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((u32) 0x80000000));
    port_outl(0xCF8, address);
    return (u8) ((port_inl(0xCFC) >> ((offset & 3) * 8)) & 0xFF);
}

static u16 pci_read16(u8 bus, u8 slot, u8 func, u8 offset)
{
    const u32 address = (u32) ((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((u32) 0x80000000));
    port_outl(0xCF8, address);
    return (u16) ((port_inl(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);
}

static void scan_bus(u8 bus);
static void scan_device(u8 bus, u8 device);
static void scan_function(u8 bus, u8 device, u8 function);

void scan_bus(u8 bus)
{
    printf("PCI: scanning bus 0x%x\n", bus);
    for (u8 device = 0; device < 32; device++)
        scan_device(bus, device);
    printf("PCI: scan complete.\n");
}

static void scan_device(u8 bus, u8 device)
{
    u8 function = 0;
    const u16 vendor = pci_read16(bus, device, function, 0);
    if (vendor == 0xFFFF)
        return;

    scan_function(bus, device, function);
    function++;

    const u8 header_type = pci_read8(bus, device, function, 0xE);
    if ((header_type & 0x80) != 0)
    {
        // check remaining functions if this is a multi-function device
        for (; function < 8; function++)
        {
            u16 vendor = pci_read16(bus, device, function, 0);
            if (vendor == 0xFFFF)
                continue;

            scan_function(bus, device, function);
        }
    }
}

static void scan_function(u8 bus, u8 device, u8 function)
{
    const u8 baseClass = pci_read8(bus, device, function, 0xB);
    const u8 subClass = pci_read8(bus, device, function, 0xA);
    const u8 progIF = pci_read8(bus, device, function, 0x9);
    const u16 deviceID = pci_read16(bus, device, function, 0x2);
    const u16 vendorID = pci_read16(bus, device, function, 0);

    const char *class_name = get_known_class_name(baseClass, subClass, progIF);
    printf("PCI: %02x:%02x.%01x: [%04x:%04x] %s (%02x:%02x:%02x)\n", bus, device, function, vendorID, deviceID, class_name, baseClass, subClass, progIF);
    free((void *) class_name);

    if ((baseClass == 0x6) && (subClass == 0x4))
    {
        const u8 secondaryBus = pci_read8(bus, device, function, 0x19);
        scan_bus(secondaryBus);
    }
}

int main(int argc, char **argv)
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    syscall_arch_syscall(X86_SYSCALL_IOPL_ENABLE, 0, 0, 0, 0);

    const u8 header_type = pci_read8(0, 0, 0, 0xE);
    if ((header_type & 0x80) == 0)
    {
        printf("PCI: single PCI host controller\n");
        scan_bus(0);
    }
    else
    {
        printf("PCI: multiple PCI host controllers\n");
        for (u8 function = 0; function < 8; function++)
        {
            u16 vendor = pci_read16(0, 0, function, 0);
            if (vendor != 0xFFFF)
                break;
            scan_bus(function);
        }
    }

    return 0;
}
