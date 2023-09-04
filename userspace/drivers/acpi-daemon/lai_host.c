// SPDX-License-Identifier: GPL-3.0-or-later

#include "lai/host.h"
#include "mos/x86/devices/port.h"

#include <fcntl.h>
#include <mos/filesystem/fs_types.h>
#include <mos/io/io_types.h>
#include <mos/mm/mm_types.h>
#include <mos/moslib_global.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <sys/stat.h>

void *laihost_malloc(size_t size)
{
    return malloc(size);
}

void *laihost_realloc(void *, size_t newsize, size_t oldsize)
{
    MOS_UNUSED(oldsize);
    return realloc(NULL, newsize);
}

void laihost_free(void *ptr, size_t)
{
    free(ptr);
}

void laihost_log(int level, const char *msg)
{
    MOS_UNUSED(level);
    puts(msg);
}

void laihost_panic(const char *msg)
{
    puts(msg);
    abort();
}

void *laihost_scan(const char *name, size_t nth)
{
    char path[MOS_PATH_MAX_LENGTH];
    if (nth)
        snprintf(path, sizeof(path), "/sys/acpi/%s%zu", name, nth);
    else
        snprintf(path, sizeof(path), "/sys/acpi/%s", name);

    printf("laihost_scan: %s...", path);

    file_stat_t statbuf;
    if (!stat(path, &statbuf))
    {
        printf("failed.\n");
        return NULL;
    }

    fd_t fd = open(path, OPEN_READ);
    if (fd == -1)
    {
        printf("failed.\n");
        return NULL;
    }

    void *ptr = syscall_mmap_file(0, statbuf.size, MEM_PERM_READ, MMAP_SHARED, fd, 0);
    syscall_io_close(fd);
    printf("ok.\n");
    return ptr;
}

// clang-format off
void laihost_outb(uint16_t port, uint8_t value) { port_outb(port, value); }
void laihost_outw(uint16_t port, uint16_t value) { port_outw(port, value); }
void laihost_outd(uint16_t port, uint32_t value) { port_outl(port, value); }
uint8_t laihost_inb(uint16_t port) { return port_inb(port); }
uint16_t laihost_inw(uint16_t port) { return port_inw(port); }
uint32_t laihost_ind(uint16_t port) { return port_inl(port); }
// clang-format on

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

static u32 pci_read32(u8 bus, u8 slot, u8 func, u8 offset)
{
    const u32 address = (u32) ((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((u32) 0x80000000));
    port_outl(0xCF8, address);
    return port_inl(0xCFC);
}

uint8_t laihost_pci_readb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset)
{
    MOS_UNUSED(seg);
    return pci_read8(bus, slot, func, offset);
}

uint16_t laihost_pci_readw(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset)
{
    MOS_UNUSED(seg);
    return pci_read16(bus, slot, func, offset);
}

uint32_t laihost_pci_readd(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset)
{
    MOS_UNUSED(seg);
    return pci_read32(bus, slot, func, offset);
}

fd_t sysmemfd = -1;

__attribute__((constructor)) static void init_sysmemfd()
{
    sysmemfd = open("/sys/mem", OPEN_READ | OPEN_WRITE);
}

void *laihost_map(size_t paddr, size_t npages)
{
    if (sysmemfd == -1)
        return NULL;

    return syscall_mmap_file(0, npages * MOS_PAGE_SIZE, MEM_PERM_READ | MEM_PERM_WRITE, MMAP_SHARED, sysmemfd, paddr);
}

void laihost_unmap(void *vaddr, size_t npages)
{
    syscall_munmap(vaddr, npages * MOS_PAGE_SIZE);
}

void laihost_sleep(uint64_t ms)
{
    MOS_UNUSED(ms);
    ;
}
