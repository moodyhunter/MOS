// SPDX-License-Identifier: GPL-3.0-or-later

#include "dm/dmrpc.h"
#include "known_devices.hpp"
#include "pci_scan.hpp"

#include <fcntl.h>
#include <librpc/rpc_client.h>
#include <mos/mm/mm_types.h>
#include <mos/syscall/usermode.h>
#include <mos/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEBUG 0

// clang-format off
#define debug_printf(fmt, ...) do { if (DEBUG) fprintf(stderr, fmt __VA_OPT__(,) __VA_ARGS__); } while (0)
// clang-format on

ptr_t mmio_base;
static rpc_server_stub_t *dm;

RPC_DECLARE_CLIENT(dm, DEVICE_MANAGER_RPCS_X)

static void scan_callback(u8 bus, u8 device, u8 function, u16 vendor_id, u16 device_id, u8 base_class, u8 sub_class, u8 prog_if)
{
    const auto class_name = get_known_class_name(base_class, sub_class, prog_if);
    debug_printf("PCI: %02x:%02x.%01x: [%04x:%04x] %s (%02x:%02x:%02x)\n", bus, device, function, vendor_id, device_id, class_name.c_str(), base_class, sub_class,
                 prog_if);

    dm_register_device(dm, vendor_id, device_id, bus, device, function, mmio_base);
}

typedef struct
{
    u64 base_address;
    u16 segment_group_number;
    u8 start_pci_bus_number;
    u8 end_pci_bus_number;
    u32 reserved;
} __packed acpi_mcfg_base_addr_alloc_t;

typedef struct
{
    char signature[4];
    u32 length;
    u8 revision;
    u8 checksum;
    char oem_id[6];
    char oem_table_id[8];
    u32 oem_revision;
    u32 creator_id;
    u32 creator_revision;
    u8 reserved[8];
    char content[];
} acpi_mcfg_header_t;

static const acpi_mcfg_header_t *mcfg_table;
static const acpi_mcfg_base_addr_alloc_t *base_addr_alloc;
static size_t n_base_addr_alloc;

static bool read_mcfg_table(void)
{
    int fd = open("/sys/acpi/MCFG", O_RDONLY);
    if (fd < 0)
    {
        puts("pci-daemon: failed to open /sys/acpi/MCFG");
        return false;
    }

    mcfg_table = (acpi_mcfg_header_t *) mmap(NULL, 4 KB, PROT_READ, MAP_PRIVATE, fd, 0);
    base_addr_alloc = (acpi_mcfg_base_addr_alloc_t *) &mcfg_table->content[0];
    close(fd);

    n_base_addr_alloc = (mcfg_table->length - sizeof(acpi_mcfg_header_t)) / sizeof(acpi_mcfg_base_addr_alloc_t);

    for (size_t i = 0; i < n_base_addr_alloc; i++)
    {
        const acpi_mcfg_base_addr_alloc_t *alloc = &base_addr_alloc[i];
        debug_printf("pci-daemon: MCFG table: base_address=%llx\n", alloc->base_address);
        debug_printf("pci-daemon: MCFG table: segment_group_number=%x\n", alloc->segment_group_number);
        debug_printf("pci-daemon: MCFG table: start_pci_bus_number=%x\n", alloc->start_pci_bus_number);
        debug_printf("pci-daemon: MCFG table: end_pci_bus_number=%x\n", alloc->end_pci_bus_number);
        debug_printf("pci-daemon: MCFG table: reserved=%x\n", alloc->reserved);
    }

    if (n_base_addr_alloc > 1)
    {
        fputs("pci-daemon: MCFG table: multiple base address allocators are not supported", stderr);
        return false;
    }

    mmio_base = base_addr_alloc->base_address;

    // memory range
    const ptr_t start = base_addr_alloc->base_address;
    const ptr_t end = start + ((u32) base_addr_alloc->end_pci_bus_number - base_addr_alloc->start_pci_bus_number + 1) * 4 KB;
    debug_printf("pci-daemon: PCI memory range: " PTR_FMT "-" PTR_FMT "\n", start, end);

    // map the PCI memory range
    const u64 size = ALIGN_UP_TO_PAGE(end - start);

    fd_t memfd = open("/sys/mem", O_RDWR);
    if (memfd < 0)
    {
        puts("pci-daemon: failed to open /sys/mem");
        return false;
    }

    void *addr = mmap((void *) start, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, memfd, start);
    close(memfd);

    MOS_UNUSED(addr);

    return true;
}

int main(int argc, char **argv)
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    if (!read_mcfg_table())
    {
        puts("pci-daemon: failed to read MCFG table");
        return 1;
    }

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
