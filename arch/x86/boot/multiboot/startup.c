// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/boot/startup.h"

#include "mos/device/console.h"
#include "mos/mos_global.h"
#include "mos/types.h"

extern const char __MOS_KERNEL_CODE_START;
extern const char __MOS_KERNEL_CODE_END;
extern const char __MOS_KERNEL_RO_START;
extern const char __MOS_KERNEL_RO_END;
extern const char __MOS_KERNEL_RW_START;
extern const char __MOS_KERNEL_RW_END;

extern const char __MOS_KERNEL_END;

__startup static void print_debug_info(char a, char b)
{
    *((char *) 0xb8000) = a;
    *((char *) 0xb8001) = LightMagenta;
    *((char *) 0xb8002) = b;
    *((char *) 0xb8003) = White;
}

__startup void x86_startup_setup()
{
    if ((uintptr_t) &__MOS_KERNEL_END > MOS_X86_HEAP_BASE_VADDR)
    {
        print_debug_info('E', '1');
        __asm__("cli; hlt;");
    }
    print_debug_info('P', 'p');
    startup_setup_paging((paging_handle_t){ 0 });

    // TODO: enable paging, map initrd & multiboot area?
}

__startup void map_pages(paging_handle_t handle, uintptr_t virt, uintptr_t phys, size_t size, vm_flags flags)
{
    MOS_UNUSED(handle);
    MOS_UNUSED(virt);
    MOS_UNUSED(phys);
    MOS_UNUSED(size);
    MOS_UNUSED(flags);
}

__startup void unmap_pages(paging_handle_t handle, uintptr_t virt, size_t size)
{
    MOS_UNUSED(handle);
    MOS_UNUSED(virt);
    MOS_UNUSED(size);
}

__startup_data const startup_ops_t mos_startup_info = {
    .code_start = (uintptr_t) &__MOS_KERNEL_CODE_START,
    .code_end = (uintptr_t) &__MOS_KERNEL_CODE_END,

    .rodata_start = (uintptr_t) &__MOS_KERNEL_RO_START,
    .rodata_end = (uintptr_t) &__MOS_KERNEL_RO_END,

    .rw_start = (uintptr_t) &__MOS_KERNEL_RW_START,
    .rw_end = (uintptr_t) &__MOS_KERNEL_RW_END,

    .page_size = 4 KB,

    .map_pages = map_pages,
    .unmap_pages = unmap_pages,
};
