// SPDX-License-Identifier: GPL-3.0-or-later

#include "pci_scan.hpp"

#include <mos/types.h>

static u8 pcie_read8(u8 bus, u8 slot, u8 func, u8 offset)
{
    const ptr_t addr = mmio_base + (bus << 20) + (slot << 15) + (func << 12) + offset; // PCI Express Extended Configuration Space
    return *((volatile u8 *) addr);
}

static u16 pci_read16(u8 bus, u8 slot, u8 func, u8 offset)
{
    const ptr_t addr = mmio_base + (bus << 20) + (slot << 15) + (func << 12) + offset;
    return *((volatile u16 *) addr);
}

void scan_bus(u8 bus, pci_scan_callback_t callback)
{
    for (u8 device = 0; device < 32; device++)
        scan_device(bus, device, callback);
}

void scan_device(u8 bus, u8 device, pci_scan_callback_t callback)
{
    u8 function = 0;
    const u16 vendor = pci_read16(bus, device, function, PCI_OFFSET_VENDOR_ID);
    if (vendor == 0xFFFF)
        return;

    scan_function(bus, device, function, callback);
    function++;

    const u8 header_type = pcie_read8(bus, device, function, PCI_OFFSET_HEADER_TYPE);
    if (header_type & PCI_HEADER_TYPE_MULTIFUNC)
    {
        // check remaining functions if this is a multi-function device
        for (; function < 8; function++)
        {
            u16 vendor = pci_read16(bus, device, function, PCI_OFFSET_VENDOR_ID);
            if (vendor == 0xFFFF)
                continue;

            scan_function(bus, device, function, callback);
        }
    }
}

void scan_function(u8 bus, u8 device, u8 function, pci_scan_callback_t callback)
{
    const u8 baseClass = pcie_read8(bus, device, function, PCI_OFFSET_BASE_CLASS);
    const u8 subClass = pcie_read8(bus, device, function, PCI_OFFSET_SUB_CLASS);
    const u8 progIF = pcie_read8(bus, device, function, PCI_OFFSET_PROG_IF);
    const u16 deviceID = pci_read16(bus, device, function, PCI_OFFSET_DEVICE_ID);
    const u16 vendorID = pci_read16(bus, device, function, PCI_OFFSET_VENDOR_ID);

    callback(bus, device, function, vendorID, deviceID, baseClass, subClass, progIF);

    if ((baseClass == 0x6) && (subClass == 0x4))
    {
        const u8 secondaryBus = pcie_read8(bus, device, function, 0x19);
        scan_bus(secondaryBus, callback);
    }
}

void scan_pci(pci_scan_callback_t callback)
{
    const u8 header_type = pcie_read8(0, 0, 0, 0xE);
    if ((header_type & 0x80) == 0)
    {
        scan_bus(0, callback);
    }
    else
    {
        for (u8 function = 0; function < 8; function++)
        {
            u16 vendor = pci_read16(0, 0, function, 0);
            if (vendor != 0xFFFF)
                break;
            scan_bus(function, callback);
        }
    }
}
