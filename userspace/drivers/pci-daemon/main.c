// SPDX-License-Identifier: GPL-3.0-or-later

#include "dm/client.h"
#include "known_devices.h"
#include "pci_scan.h"

#include <librpc/rpc_client.h>
#include <mos/types.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>
#include <sys/stat.h>

static rpc_server_stub_t *dm;

#define PCI_RPC_SERVER_NAME "drivers.pci"

void scan_callback(u8 bus, u8 device, u8 function, u16 vendor_id, u16 device_id, u8 base_class, u8 sub_class, u8 prog_if)
{
    const char *class_name = get_known_class_name(base_class, sub_class, prog_if);
    printf("PCI: %02x:%02x.%01x: [%04x:%04x] %s (%02x:%02x:%02x)\n", bus, device, function, vendor_id, device_id, class_name, base_class, sub_class, prog_if);
    free((void *) class_name);

    const u32 location = (bus << 16) | (device << 8) | function;
    dm_register_device(dm, vendor_id, device_id, location, PCI_RPC_SERVER_NAME);
}

int main(int argc, char **argv)
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    dm = rpc_client_create(MOS_DEVICE_MANAGER_SERVICE_NAME);
    if (!dm)
    {
        printf("pci-daemon: failed to connect to device manager\n");
        return 1;
    }

    syscall_arch_syscall(X86_SYSCALL_IOPL_ENABLE, 0, 0, 0, 0);
    scan_pci(scan_callback);

    return 0;
}
