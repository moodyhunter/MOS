// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mos_global.h>
#include <mos/platform/platform.h>
#include <mos/types.h>

#define __startup_code   __section(".mos.startup.text")
#define __startup_rodata __section(".mos.startup.rodata")
#define __startup_rwdata __section(".mos.startup.data")

// map a single page of a given physical address to a given virtual address
// vaddr and paddr must be page-aligned
__startup_code void mos_startup_map_single_page(ptr_t vaddr, ptr_t paddr, vm_flags flags);

__startup_code always_inline void mos_startup_memzero(void *start, size_t size)
{
    u8 *ptr = start;
    for (size_t i = 0; i < size; i++)
        ptr[i] = 0;
}

__startup_code always_inline size_t mos_startup_strlen(const char *str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

__startup_code always_inline void mos_startup_map_pages(ptr_t vaddr, ptr_t paddr, size_t npages, vm_flags flags)
{
    for (size_t i = 0; i < npages; i++)
        mos_startup_map_single_page(vaddr + i * MOS_PAGE_SIZE, paddr + i * MOS_PAGE_SIZE, flags);
}

__startup_code always_inline void mos_startup_map_bytes(ptr_t vaddr, ptr_t paddr, size_t nbytes, vm_flags flags)
{
    paddr = ALIGN_DOWN_TO_PAGE(paddr);
    const ptr_t aligned_vaddr = ALIGN_DOWN_TO_PAGE(vaddr);
    const size_t npages = ALIGN_UP_TO_PAGE(nbytes + (vaddr - aligned_vaddr)) / MOS_PAGE_SIZE;
    mos_startup_map_pages(aligned_vaddr, paddr, npages, flags);
}

__startup_code always_inline void mos_startup_map_identity(ptr_t paddr, size_t nbytes, vm_flags flags)
{
    paddr = ALIGN_DOWN_TO_PAGE(paddr);
    mos_startup_map_bytes(paddr, paddr, nbytes, flags);
}

__startup_code always_inline void mos_startup_map_bios(ptr_t paddr, size_t nbytes, vm_flags flags)
{
    paddr = ALIGN_DOWN_TO_PAGE(paddr);
    mos_startup_map_bytes(BIOS_VADDR(paddr), paddr, nbytes, flags | VM_CACHE_DISABLED);
}
