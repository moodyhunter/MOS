// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/boot/startup.h"

#include "mos/device/console.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/types.h"
#include "mos/x86/mm/paging_impl.h"
#include "mos/x86/x86_platform.h"

#define ALIGN_UP_TO(addr, size) (((addr) + size - 1) & ~(size - 1))

extern const char __MOS_STARTUP_START;
extern const char __MOS_STARTUP_END;

extern const char __MOS_KERNEL_CODE_START;
extern const char __MOS_KERNEL_CODE_END;
extern const char __MOS_KERNEL_RO_START;
extern const char __MOS_KERNEL_RO_END;
extern const char __MOS_KERNEL_RW_START;
extern const char __MOS_KERNEL_RW_END;

extern const char __MOS_KERNEL_END;

extern const char __MOS_STARTUP_PGD;
extern const char __MOS_STARTUP_PGTABLE_0;

__startup_data static const uintptr_t startup_start = (uintptr_t) &__MOS_STARTUP_START;
__startup_data static const uintptr_t startup_end = (uintptr_t) &__MOS_STARTUP_END;

__startup_data static const uintptr_t kernel_code_vstart = (uintptr_t) &__MOS_KERNEL_CODE_START;
__startup_data static const uintptr_t kernel_code_vend = (uintptr_t) &__MOS_KERNEL_CODE_END;
__startup_data static const uintptr_t kernel_ro_vstart = (uintptr_t) &__MOS_KERNEL_RO_START;
__startup_data static const uintptr_t kernel_ro_vend = (uintptr_t) &__MOS_KERNEL_RO_END;
__startup_data static const uintptr_t kernel_rw_vstart = (uintptr_t) &__MOS_KERNEL_RW_START;
__startup_data static const uintptr_t kernel_rw_vend = (uintptr_t) &__MOS_KERNEL_RW_END;

__startup_data static x86_pgdir_entry *const startup_pgd = (x86_pgdir_entry *) &__MOS_STARTUP_PGD;
__startup_data static x86_pgtable_entry *const pages = (x86_pgtable_entry *) &__MOS_STARTUP_PGTABLE_0;

__startup void memzero(void *start, size_t size)
{
    u8 *ptr = start;
    for (size_t i = 0; i < size; i++)
        ptr[i] = 0;
}

__startup static void print_debug_info(char a, char b)
{
    *((char *) 0xb8000) = a;
    *((char *) 0xb8001) = LightMagenta;
    *((char *) 0xb8002) = b;
    *((char *) 0xb8003) = White;
    *((char *) 0xb8004) = '\0';
    *((char *) 0xb8005) = White;
}

__startup void startup_map_page(x86_pgtable_entry *this_table, uintptr_t vaddr, uintptr_t paddr, vm_flags flags)
{
    size_t page_dir_index = vaddr >> 22;
    x86_pgdir_entry *this_dir = &startup_pgd[page_dir_index];
    memzero(this_table, sizeof(x86_pgtable_entry));

    if (unlikely(!this_dir->present))
    {
        memzero(this_dir, sizeof(x86_pgdir_entry));
        this_dir->present = true;
        this_dir->page_table_paddr = (uintptr_t) this_table >> 12;
    }

    this_table->present = true;
    this_table->phys_addr = (uintptr_t) paddr >> 12;

    this_dir->writable = flags & VM_WRITE;
    this_table->writable = flags & VM_WRITE;

    this_table->global = flags & VM_GLOBAL;

    __asm__ volatile("invlpg (%0)" ::"r"(vaddr) : "memory");
}

__startup void startup_map_pages(x86_pgtable_entry *pgt, uintptr_t vaddr, uintptr_t paddr, size_t n, vm_flags f)
{
    for (size_t i = 0; i < n; i++)
        startup_map_page(pgt + i, vaddr + i * X86_PAGE_SIZE, paddr + i * X86_PAGE_SIZE, f);
}

__startup void x86_startup_setup()
{
    char step = '1';
#define debug_print() print_debug_info('S', step++)
    debug_print();

    size_t pgindex = 0;

    // skip the free list setup, use do_ version
    startup_map_page(&pages[pgindex], 0, 0, VM_NONE); // ! the zero page is not writable
    pgindex++;

    startup_map_pages(&pages[pgindex], X86_PAGE_SIZE, X86_PAGE_SIZE, 1 MB / X86_PAGE_SIZE - 1, VM_GLOBAL | VM_WRITE);
    pgindex += 1 MB / X86_PAGE_SIZE - 1;

    const uintptr_t startup_code_size = X86_ALIGN_UP_TO_PAGE(startup_end - startup_start);
    const uintptr_t startup_pgsize = startup_code_size / X86_PAGE_SIZE;
    startup_map_pages(&pages[pgindex], startup_start, startup_start, startup_pgsize, VM_WRITE | VM_EXECUTE);
    pgindex += startup_pgsize;

    const uintptr_t kernel_code_size = X86_ALIGN_UP_TO_PAGE(kernel_code_vend - kernel_code_vstart);
    const uintptr_t vaddr_code = MOS_KERNEL_START_VADDR;
    const size_t kernel_code_pgsize = kernel_code_size / X86_PAGE_SIZE;
    startup_map_pages(&pages[pgindex], vaddr_code, kernel_code_vstart - 0xc0000000, kernel_code_pgsize, VM_GLOBAL | VM_EXECUTE);
    pgindex += kernel_code_pgsize;

    const uintptr_t kernel_ro_size = X86_ALIGN_UP_TO_PAGE(kernel_ro_vend - kernel_ro_vstart);
    const uintptr_t vaddr_rodata = X86_ALIGN_UP_TO_PAGE(vaddr_code + kernel_code_size);
    const size_t kernel_ro_pgsize = kernel_ro_size / X86_PAGE_SIZE;
    startup_map_pages(&pages[pgindex], vaddr_rodata, kernel_ro_vstart - 0xc0000000, kernel_ro_pgsize, VM_GLOBAL);
    pgindex += kernel_ro_pgsize;

    const uintptr_t kernel_rw_size = X86_ALIGN_UP_TO_PAGE(kernel_rw_vend - kernel_rw_vstart);
    const uintptr_t vaddr_rw = X86_ALIGN_UP_TO_PAGE(vaddr_rodata + kernel_ro_size);
    const size_t kernel_rw_pgsize = kernel_rw_size / X86_PAGE_SIZE;
    startup_map_pages(&pages[pgindex], vaddr_rw, kernel_rw_vstart - 0xc0000000, kernel_rw_pgsize, VM_GLOBAL | VM_WRITE);
    pgindex += kernel_rw_pgsize;

    // load the page directory
    debug_print();
    __asm__ volatile("mov %0, %%cr3" ::"r"(startup_pgd));

    // enable paging
    debug_print();
    __asm__ volatile("mov %%cr0, %%eax; or $0x80000000, %%eax; mov %%eax, %%cr0" ::: "eax");

    // enable global pages
    debug_print();
    __asm__ volatile("mov %%cr4, %%eax; or $0x00000080, %%eax; mov %%eax, %%cr4" ::: "eax");

    debug_print();
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
