// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "mos/printk.h"
#include "mos/x86/mm/page_entry.h"

#define X86_PAGE_OR_DIR_SIZE 1024

#define MEM_ALIGNMENT_4K 4096

static page_directory_entry page_dir[X86_PAGE_OR_DIR_SIZE] __aligned(MEM_ALIGNMENT_4K) = { 0 };
static page_table_entry kernel_table[X86_PAGE_OR_DIR_SIZE] __aligned(MEM_ALIGNMENT_4K) = { 0 };

extern void x86_enable_paging(void *page_dir);

void paging_setup()
{
    // Only set the writable field to true.
    for (s32 i = 0; i < X86_PAGE_OR_DIR_SIZE; i++)
        page_dir[i].writable = true;

    // The kernel page
    for (s32 i = 0; i < X86_PAGE_OR_DIR_SIZE; i++)
    {
        // Only set the writable field to true.
        kernel_table[i].present = true;
        kernel_table[i].writable = true;
        // ! should have '* MEM_ALIGNMENT_4K >> 12', but that's not necessary
        kernel_table[i].mem_addr = i;
    }

    page_dir[0].present = true;
    page_dir[0].writable = true;
    page_dir[0].table_address = (u32) kernel_table >> 12;

    pr_info("Page directory: %p", (void *) &page_dir[0]);
    x86_enable_paging(page_dir);
}
