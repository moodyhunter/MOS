// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/boot/startup.h"

#include "mos/device/console.h"
#include "mos/kconfig.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/types.h"
#include "mos/x86/acpi/acpi_types.h"
#include "mos/x86/boot/multiboot.h"
#include "mos/x86/mm/paging_impl.h"
#include "mos/x86/x86_platform.h"

#define X86_VIDEO_DEVICE 0xb8000
#define VIDEO_WIDTH      80
#define VIDEO_HEIGHT     25

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
extern const char __MOS_STARTUP_PGTABLE;

__startup_rodata static const uintptr_t startup_start = (uintptr_t) &__MOS_STARTUP_START;
__startup_rodata static const uintptr_t startup_end = (uintptr_t) &__MOS_STARTUP_END;

__startup_rodata static const uintptr_t kernel_code_vstart = (uintptr_t) &__MOS_KERNEL_CODE_START;
__startup_rodata static const uintptr_t kernel_code_vend = (uintptr_t) &__MOS_KERNEL_CODE_END;
__startup_rodata static const uintptr_t kernel_ro_vstart = (uintptr_t) &__MOS_KERNEL_RO_START;
__startup_rodata static const uintptr_t kernel_ro_vend = (uintptr_t) &__MOS_KERNEL_RO_END;
__startup_rodata static const uintptr_t kernel_rw_vstart = (uintptr_t) &__MOS_KERNEL_RW_START;
__startup_rodata static const uintptr_t kernel_rw_vend = (uintptr_t) &__MOS_KERNEL_RW_END;

__startup_rodata static x86_pgdir_entry *const startup_pgd = (x86_pgdir_entry *) &__MOS_STARTUP_PGD;
__startup_rodata static x86_pgtable_entry *const pages = (x86_pgtable_entry *) &__MOS_STARTUP_PGTABLE;

__startup_rwdata volatile size_t initrd_size = 0;
__startup_rwdata uintptr_t video_device_address = X86_VIDEO_DEVICE;

#define STARTUP_ASSERT(cond, type)                                                                                                              \
    do                                                                                                                                          \
    {                                                                                                                                           \
        if (!unlikely(cond))                                                                                                                    \
        {                                                                                                                                       \
            print_debug_info('E', type);                                                                                                        \
            while (1)                                                                                                                           \
                __asm__("hlt");                                                                                                                 \
        }                                                                                                                                       \
    } while (0)

#define debug_print_step() print_debug_info('S', step++)

__startup_code should_inline void startup_memzero(void *start, size_t size)
{
    u8 *ptr = start;
    for (size_t i = 0; i < size; i++)
        ptr[i] = 0;
}

__startup_code should_inline size_t startup_strlen(const char *str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

__startup_code should_inline void print_debug_info(char a, char b)
{
    *((char *) video_device_address + 0) = a;
    *((char *) video_device_address + 1) = LightMagenta;
    *((char *) video_device_address + 2) = b;
    *((char *) video_device_address + 3) = Gray;
    *((char *) video_device_address + 4) = '\0';
    *((char *) video_device_address + 5) = White;
}

__startup_code should_inline void startup_setup_pgd(int pgdid, x86_pgtable_entry *pgtable)
{
    STARTUP_ASSERT(pgdid < 1024, 'r');                    // pgdid must be less than 1024
    STARTUP_ASSERT(pgtable != NULL, 't');                 // pgtable must not be NULL
    STARTUP_ASSERT((uintptr_t) pgtable % 4096 == 0, 'a'); // pgtable must be aligned to 4096
    STARTUP_ASSERT(!startup_pgd[pgdid].present, 'p');     // pgdir entry already present

    startup_memzero(startup_pgd + pgdid, sizeof(x86_pgdir_entry));
    startup_pgd[pgdid].present = true;
    startup_pgd[pgdid].page_table_paddr = (uintptr_t) pgtable >> 12;
}

__startup_code should_inline void startup_map_page(uintptr_t vaddr, uintptr_t paddr, vm_flags flags)
{
    const size_t dir_index = vaddr >> 22;
    const size_t table_index = (vaddr >> 12) & 0x3FF;

    __startup_rwdata static int used_pgd = 0;

    x86_pgdir_entry *this_dir = &startup_pgd[dir_index];
    if (!this_dir->present)
    {
        size_t pagedir_entry_table_index = 0;

        if (vaddr - (dir_index << 22) > 0)
            pagedir_entry_table_index = used_pgd * 1024;
        else if (vaddr - (dir_index << 22) == 0)
            pagedir_entry_table_index = ALIGN_UP(used_pgd * 1024, 1024);
        else
            STARTUP_ASSERT(false, 'v');

        startup_setup_pgd(dir_index, &pages[pagedir_entry_table_index]);
        used_pgd++;
    }

    // this_dir should be present now
    STARTUP_ASSERT(this_dir->present, 'm');
    this_dir->writable = flags & VM_WRITE;

    x86_pgtable_entry *this_table = (x86_pgtable_entry *) (this_dir->page_table_paddr << 12) + table_index;
    startup_memzero(this_table, sizeof(x86_pgtable_entry));
    this_table->present = true;
    this_table->phys_addr = (uintptr_t) paddr >> 12;
    this_table->writable = flags & VM_WRITE;
    this_table->global = flags & VM_GLOBAL;

    __asm__ volatile("invlpg (%0)" ::"r"(vaddr) : "memory");
}

__startup_code should_inline void startup_map_pages(uintptr_t vaddr, uintptr_t paddr, size_t npages, vm_flags flags)
{
    for (size_t i = 0; i < npages; i++)
        startup_map_page(vaddr + i * X86_PAGE_SIZE, paddr + i * X86_PAGE_SIZE, flags);
}

__startup_code should_inline void startup_map(uintptr_t vaddr, uintptr_t paddr, size_t nbytes, vm_flags flags)
{
    paddr = X86_ALIGN_DOWN_TO_PAGE(paddr);
    const uintptr_t start_vaddr = X86_ALIGN_DOWN_TO_PAGE(vaddr);
    const size_t npages = X86_ALIGN_UP_TO_PAGE(vaddr + nbytes - start_vaddr) / X86_PAGE_SIZE;
    startup_map_pages(start_vaddr, paddr, npages, flags);
}

__startup_code should_inline void startup_map_identity(uintptr_t paddr, size_t nbytes, vm_flags flags)
{
    paddr = X86_ALIGN_DOWN_TO_PAGE(paddr);
    startup_map(paddr, paddr, nbytes, flags);
}

__startup_code should_inline void startup_map_bios(uintptr_t paddr, size_t nbytes, vm_flags flags)
{
    paddr = X86_ALIGN_DOWN_TO_PAGE(paddr);
    const uintptr_t vaddr = X86_BIOS_VADDR_MASK | (paddr & ~0xF0000000);
    startup_map(vaddr, paddr, nbytes, flags);
}

// x86_startup does the following:
// 1. Identity map the VGA buffer to kvirt address space
// 2. Identity map the code section '.mos.startup*'
// 3. Map the kernel code, rodata, data, bss and kpage tables
// * 4. Find the initrd and map it
// * 5. Find a possible location for the kernel stack and map it
// 4. Enable paging
__startup_code asmlinkage void x86_startup(u32 magic, multiboot_info_t *mb_info)
{
    char step = 'a';

    STARTUP_ASSERT(magic == MULTIBOOT_BOOTLOADER_MAGIC, '1');
    STARTUP_ASSERT(mb_info->flags & MULTIBOOT_INFO_MEM_MAP, '2');

    startup_memzero(startup_pgd, sizeof(x86_pgdir_entry) * 1024);

    debug_print_step();
    startup_map_identity((uintptr_t) mb_info, sizeof(multiboot_info_t), VM_NONE);

    // multiboot stuff
    if (mb_info->flags & MULTIBOOT_INFO_CMDLINE)
        startup_map_identity((uintptr_t) mb_info->cmdline, startup_strlen((char *) mb_info->cmdline), VM_NONE);

    STARTUP_ASSERT(mb_info->mmap_addr, 'm');
    startup_map_identity((uintptr_t) mb_info->mmap_addr, mb_info->mmap_length * sizeof(multiboot_mmap_entry_t), VM_NONE);

    // map the VGA buffer, from 0xB8000
    startup_map_bios(X86_VIDEO_DEVICE, VIDEO_WIDTH * VIDEO_HEIGHT * 2, VM_WRITE);

    // map the bios memory regions
    startup_map_bios(X86_BIOS_MEMREGION_PADDR, BIOS_MEMREGION_SIZE, VM_NONE);
    startup_map_bios(X86_EBDA_MEMREGION_PADDR, EBDA_MEMREGION_SIZE, VM_NONE);

    // ! we do not separate the startup code and data to simplify the setup.
    // ! this page directory will be removed as soon as the kernel is loaded, it shouldn't be a problem.
    startup_map_identity(startup_start, startup_end - startup_start, VM_WRITE | VM_EXECUTE);

    debug_print_step();
    const size_t kernel_code_pgsize = X86_ALIGN_UP_TO_PAGE(kernel_code_vend - kernel_code_vstart) / X86_PAGE_SIZE;
    startup_map_pages(kernel_code_vstart, kernel_code_vstart - MOS_KERNEL_START_VADDR, kernel_code_pgsize, VM_EXECUTE);

    const size_t kernel_ro_pgsize = X86_ALIGN_UP_TO_PAGE(kernel_ro_vend - kernel_ro_vstart) / X86_PAGE_SIZE;
    startup_map_pages(kernel_ro_vstart, kernel_ro_vstart - MOS_KERNEL_START_VADDR, kernel_ro_pgsize, VM_NONE);

    const size_t kernel_rw_pgsize = X86_ALIGN_UP_TO_PAGE(kernel_rw_vend - kernel_rw_vstart) / X86_PAGE_SIZE;
    startup_map_pages(kernel_rw_vstart, kernel_rw_vstart - MOS_KERNEL_START_VADDR, kernel_rw_pgsize, VM_WRITE);

    if (mb_info->flags & MULTIBOOT_INFO_MODS && mb_info->mods_count != 0)
    {
        multiboot_module_t *mod = (multiboot_module_t *) mb_info->mods_addr;
        const size_t initrd_pgsize = X86_ALIGN_UP_TO_PAGE(mod->mod_end - mod->mod_start) / X86_PAGE_SIZE;
        initrd_size = mod->mod_end - mod->mod_start;
        startup_map_pages(MOS_X86_INITRD_VADDR, mod->mod_start, initrd_pgsize, VM_NONE);
        debug_print_step();
    }

    __asm__ volatile("mov %0, %%cr3" ::"r"(startup_pgd));
    debug_print_step();

    __asm__ volatile("mov %%cr0, %%eax; or $0x80000000, %%eax; mov %%eax, %%cr0" ::: "eax");
    video_device_address = X86_VIDEO_DEVICE | X86_BIOS_VADDR_MASK;
    debug_print_step();

    // find the acpi rsdp after paging is enabled
    acpi_rsdp_t *rsdp = find_acpi_rsdp(X86_EBDA_MEMREGION_PADDR | X86_BIOS_VADDR_MASK, EBDA_MEMREGION_SIZE);
    if (!rsdp)
        rsdp = find_acpi_rsdp(X86_BIOS_MEMREGION_PADDR | X86_BIOS_VADDR_MASK, BIOS_MEMREGION_SIZE);

    STARTUP_ASSERT(rsdp, 'r');

    const multiboot_mmap_entry_t *map_entries = (multiboot_mmap_entry_t *) mb_info->mmap_addr;
    for (u32 i = 0; i < mb_info->mmap_length / sizeof(multiboot_mmap_entry_t); i++)
    {
        u64 region_length = map_entries[i].len;
        u64 region_base = map_entries[i].phys_addr;
        if (rsdp->v1.rsdt_addr >= region_base && rsdp->v1.rsdt_addr < region_base + region_length)
        {
            startup_map_bios(region_base, region_length, VM_NONE);
            break;
        }
    }
    debug_print_step();
}
