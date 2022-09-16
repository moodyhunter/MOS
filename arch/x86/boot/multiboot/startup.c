// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/boot/startup.h"

#include "mos/device/console.h"
#include "mos/mos_global.h"
#include "mos/types.h"
#include "mos/x86/boot/multiboot.h"
#include "mos/x86/mm/paging_impl.h"

#define ALIGN_UP_TO(addr, size) (((addr) + size - 1) & ~(size - 1))

#define VIDEO_DEVICE_ADDRESS 0xB8000
#define VIDEO_WIDTH          80
#define VIDEO_HEIGHT         25

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

__startup_rwdata volatile uintptr_t kernel_higher_stack_top = 0;
__startup_rwdata volatile uintptr_t kernel_higher_initrd_addr = 0;
__startup_rwdata volatile size_t kernel_higher_initrd_size = 0;

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

__startup_code static inline void memzero(void *start, size_t size)
{
    u8 *ptr = start;
    for (size_t i = 0; i < size; i++)
        ptr[i] = 0;
}

__startup_code static inline void print_debug_info(char a, char b)
{
    *((char *) VIDEO_DEVICE_ADDRESS + 0) = a;
    *((char *) VIDEO_DEVICE_ADDRESS + 1) = LightMagenta;
    *((char *) VIDEO_DEVICE_ADDRESS + 2) = b;
    *((char *) VIDEO_DEVICE_ADDRESS + 3) = White;
    *((char *) VIDEO_DEVICE_ADDRESS + 4) = '\0';
    *((char *) VIDEO_DEVICE_ADDRESS + 5) = White;
}

__startup_code static inline void startup_setup_pgd(int pgdid, x86_pgtable_entry *pgtable)
{
    STARTUP_ASSERT(pgdid < 1024, 'r');                    // pgdid must be less than 1024
    STARTUP_ASSERT(pgtable != NULL, 't');                 // pgtable must not be NULL
    STARTUP_ASSERT((uintptr_t) pgtable % 4096 == 0, 'a'); // pgtable must be aligned to 4096
    STARTUP_ASSERT(!startup_pgd[pgdid].present, 'p');     // pgdir entry already present

    startup_pgd[pgdid] = (x86_pgdir_entry){
        .present = 1,
        .writable = 0,
        .usermode = 0,
        .write_through = 0,
        .cache_disabled = 0,
        .accessed = 0,
        .page_table_paddr = (uintptr_t) pgtable >> 12,
    };
}

__startup_code static void startup_map_page(uintptr_t vaddr, uintptr_t paddr, vm_flags flags)
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
            pagedir_entry_table_index = ALIGN_UP_TO(used_pgd * 1024, 1024);

        startup_setup_pgd(dir_index, &pages[pagedir_entry_table_index]);
        used_pgd++;
    }

    // this_dir should be present now
    STARTUP_ASSERT(this_dir->present, 'm');
    this_dir->writable = flags & VM_WRITE;

    x86_pgtable_entry *this_table = &pages[(used_pgd - 1) * 1024 + table_index];
    memzero(this_table, sizeof(x86_pgtable_entry));
    this_table->present = true;
    this_table->phys_addr = (uintptr_t) paddr >> 12;
    this_table->writable = flags & VM_WRITE;
    this_table->global = flags & VM_GLOBAL;

    __asm__ volatile("invlpg (%0)" ::"r"(vaddr) : "memory");
}

__startup_code static inline void startup_map_pages(uintptr_t vaddr, uintptr_t paddr, size_t n, vm_flags f)
{
    for (size_t i = 0; i < n; i++)
        startup_map_page(vaddr + i * X86_PAGE_SIZE, paddr + i * X86_PAGE_SIZE, f);
}

// x86_startup_setup does the following:
// 1. Identity map the VGA buffer to kvirt address space
// 2. Identity map the code section '.mos.startup*'
// 3. Map the kernel code, rodata, data, bss and kpage tables
// * 4. Find the initrd and map it
// * 5. Find a possible location for the kernel stack and map it
// 4. Enable paging, (+global pages enabled)
__startup_code asmlinkage void x86_startup(u32 magic, multiboot_info_t *mb_info)
{
    char step = '0';

    STARTUP_ASSERT(magic == MULTIBOOT_BOOTLOADER_MAGIC, '1');
    STARTUP_ASSERT(mb_info->flags & MULTIBOOT_INFO_MEM_MAP, '2');

    memzero(startup_pgd, sizeof(x86_pgdir_entry) * 1024);
    debug_print_step();

    const size_t video_buffer_pgsize = (VIDEO_WIDTH * VIDEO_HEIGHT * 2) / X86_PAGE_SIZE + 1;
    startup_map_pages(VIDEO_DEVICE_ADDRESS, VIDEO_DEVICE_ADDRESS, video_buffer_pgsize, VM_WRITE);
    debug_print_step();

    // ! we do not separate the startup code and data to simplify the setup.
    // ! this page directory will be removed as soon as the kernel is loaded, it shouldn't be a problem.
    const size_t startup_pgsize = X86_ALIGN_UP_TO_PAGE(startup_end - startup_start) / X86_PAGE_SIZE;
    startup_map_pages(startup_start, startup_start, startup_pgsize, VM_WRITE | VM_EXECUTE);
    debug_print_step();

    const size_t kernel_code_pgsize = X86_ALIGN_UP_TO_PAGE(kernel_code_vend - kernel_code_vstart) / X86_PAGE_SIZE;
    startup_map_pages(kernel_code_vstart, kernel_code_vstart - MOS_KERNEL_START_VADDR, kernel_code_pgsize, VM_EXECUTE);
    debug_print_step();

    const size_t kernel_ro_pgsize = X86_ALIGN_UP_TO_PAGE(kernel_ro_vend - kernel_ro_vstart) / X86_PAGE_SIZE;
    startup_map_pages(kernel_ro_vstart, kernel_ro_vstart - MOS_KERNEL_START_VADDR, kernel_ro_pgsize, VM_NONE);
    debug_print_step();

    const size_t kernel_rw_pgsize = X86_ALIGN_UP_TO_PAGE(kernel_rw_vend - kernel_rw_vstart) / X86_PAGE_SIZE;
    startup_map_pages(kernel_rw_vstart, kernel_rw_vstart - MOS_KERNEL_START_VADDR, kernel_rw_pgsize, VM_WRITE);
    debug_print_step();

    // load the page directory
    __asm__ volatile("mov %0, %%cr3" ::"r"(startup_pgd));
    debug_print_step();

    // enable paging
    __asm__ volatile("mov %%cr0, %%eax; or $0x80000000, %%eax; mov %%eax, %%cr0" ::: "eax");
    debug_print_step();
}
