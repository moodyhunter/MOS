// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/boot/startup.h"

#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/types.h"

#define ALIGN_UP_TO(addr, size) (((addr) + size - 1) & ~(size - 1))

#if MOS_32BITS
#define MOS_KERNEL_START_VADDR 0xC0000000
#else
#define MOS_KERNEL_START_VADDR 0xFFFFFFFF80000000
#endif

__startup static inline uintptr_t startup_align_up_to_page(uintptr_t value)
{
    return ALIGN_UP_TO(value, mos_startup_info.page_size);
}

__startup void startup_setup_paging(paging_handle_t handle)
{
    const uintptr_t kvaddr_code = MOS_KERNEL_START_VADDR;
    const uintptr_t kvaddr_rodata = startup_align_up_to_page(kvaddr_code + (mos_startup_info.code_end - mos_startup_info.code_start));
    const uintptr_t kvaddr_rw = startup_align_up_to_page(kvaddr_rodata + (mos_startup_info.rodata_end - mos_startup_info.rodata_start));

    const size_t kernel_code_pgsize = (mos_startup_info.code_start - mos_startup_info.code_end) / mos_startup_info.page_size;
    const size_t kernel_ro_pgsize = (mos_startup_info.rodata_end - mos_startup_info.rodata_start) / mos_startup_info.page_size;
    const size_t kernel_rw_pgsize = (mos_startup_info.rw_end - mos_startup_info.rw_start) / mos_startup_info.page_size;

    mos_startup_info.map_pages(handle, kvaddr_rodata, mos_startup_info.code_start, kernel_code_pgsize, VM_EXECUTABLE | VM_GLOBAL);
    mos_startup_info.map_pages(handle, kvaddr_rodata, mos_startup_info.rodata_start, kernel_ro_pgsize, VM_NONE | VM_GLOBAL);
    mos_startup_info.map_pages(handle, kvaddr_rw, mos_startup_info.rw_start, kernel_rw_pgsize, VM_WRITE | VM_GLOBAL);
}
