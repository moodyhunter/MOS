// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>

#define PCI_OFFSET_VENDOR_ID   0x00
#define PCI_OFFSET_DEVICE_ID   0x02
#define PCI_OFFSET_PROG_IF     0x09
#define PCI_OFFSET_SUB_CLASS   0x0A
#define PCI_OFFSET_BASE_CLASS  0x0B
#define PCI_OFFSET_HEADER_TYPE 0x0E

#define PCI_HEADER_TYPE_MULTIFUNC 0x80

typedef void (*pci_scan_callback_t)(u8 bus, u8 device, u8 function, u16 vendor_id, u16 device_id, u8 base_class, u8 sub_class, u8 prog_if);

extern ptr_t mmio_base;

void scan_pci(pci_scan_callback_t callback); // scan all buses

void scan_bus(u8 bus, pci_scan_callback_t callback);
void scan_device(u8 bus, u8 device, pci_scan_callback_t callback);
void scan_function(u8 bus, u8 device, u8 function, pci_scan_callback_t callback);
