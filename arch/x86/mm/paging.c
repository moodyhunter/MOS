// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/paging.h"

#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/x86/boot/multiboot.h"
#include "mos/x86/x86_platform.h"

#define MEM_ALIGNMENT_4K 4096

static page_directory_entry page_dir[1024] __aligned(MEM_ALIGNMENT_4K) = { 0 };
// static page_table_entry kernel_table[1024] __aligned(MEM_ALIGNMENT_4K) = { 0 };

extern void x86_enable_paging_impl(void *page_dir);

#define SET(block)                                                                                                                              \
    memory_bitmap[((block) / 32)] |= (uint32_t) (1 << ((block) % 32));                                                                          \
    s_freePages -= 1

static int s_totalPages; //< The total number of pages available.
static int s_freePages;  //< The total number of free pages.
static int s_lastPage;   //< The last page used in the bitmap.

u64 parse_memory_map(multiboot_mmap_entry_t *mboot, u32 count);

void x86_enable_paging()
{
    mos_warn("pading not implemented");
    return;
    pr_info("Page directory: %p", (void *) &page_dir[0]);
    x86_enable_paging_impl(page_dir);
}

void *x86_alloc_page(size_t n)
{
    MOS_UNUSED(n);
    return NULL;
}

bool x86_free_page(void *ptr, size_t n)
{
    MOS_UNUSED(ptr);
    MOS_UNUSED(n);
    return false;
}

// --------- ACTUAL PHYSICAL MEMORY BITMAP ----------------------
// Please go ALL THE WAY DOWN for the initialization routines.

// Basically 2 GB worth of pages. (2 GB / 4096 => 2^19)
// And 2^14 uint32_t required to manage that.

#define TOTAL_PAGES (2 << 19)
#define MAXMAP      (2 << 14)

static uint32_t memory_bitmap[MAXMAP];

#define ISSET(block) ((memory_bitmap[((block) / 32)] & (uint32_t) (1 << ((block) % 32))) != 0)

#define UNSET(block)                                                                                                                            \
    memory_bitmap[((block) / 32)] &= ~((uint32_t) (1 << ((block) % 32)));                                                                       \
    s_freePages += 1

void *memory_multiple_alloc(int pages)
{
    int i, j;
    int block = -1;

    for (i = 0; i < MAXMAP; i++)
    {
        int pos = (i + s_lastPage) % MAXMAP;
        if ((pos + pages) > MAXMAP)
        {
            i += pages - 2;
            continue;
        }
        // Contiguous does not mean wrapped-around.

        // Scan from the current pos up for pages amount.
        block = pos;
        for (j = 0; j < pages; j++)
        {
            if (ISSET(pos + j))
            {
                block = -1;
                i = i + j; // Don't waste time.
                break;
            }
        }

        if (block != -1)
            break; // Hooray. We have our range.
    }

    // Nothing was found..
    if (block == -1)
        return NULL;

    // for (i = 0; i < pages; i++)
    // {
    //     SET(block + i);
    // }

    s_lastPage = block + pages;

    return (void *) ((unsigned int) block * X86_PAGE_SIZE);
}

void *memory_alloc(int pages)
{
    void *ptr = NULL;
    MOS_ASSERT(pages > 0);

    // begin_critical_section();

    // Not enough memory.
    if (s_freePages > pages)
    {
        ptr = memory_multiple_alloc(pages);
        if (ptr == NULL)
            pr_warn("!OUT OF MEMORY. RETURNING NULL (%i,%i,%i)\n", s_totalPages, s_freePages, pages);
    }

    // end_critical_section();
    return ptr;
}

/** Unmarks the bit in the bitmap and pushes the
 * available page back onto the stack.
 */
void memory_free(int pages, void *position)
{
    int start, index;

    // begin_critical_section();
    MOS_ASSERT((((uintptr_t) position % X86_PAGE_SIZE) == 0));
    start = ((uintptr_t) position) / X86_PAGE_SIZE;

    for (index = 0; index < pages; index++)
    {
        int block = start + index;
        MOS_ASSERT(ISSET(block));
        UNSET(block);
    }
    // end_critical_section();
}

// s_totalPages = 0;
// s_freePages = 0;
// s_lastPage = 0;

// parse_memory_map(map_entry, count);

// pr_info("Reserving first megabyte of memory.");
// for (int i = 0; i < 256; i++)
// {
//     kernel_table[i].present = true;
// }

// // pr_info("Reserving kernel memory: %x - %x", KERNEL_START, KERNEL_END);
// // for (u32 i = (KERNEL_START / X86_PAGE_OR_DIR_SIZE); i < (KERNEL_END / X86_PAGE_OR_DIR_SIZE); i++)
// //     SET(i);

// dump_memory_map();

// // Only set the writable field to true.
// for (s32 i = 0; i < X86_PAGE_OR_DIR_SIZE; i++)
//     page_dir[i].writable = true;

// // The kernel page
// for (s32 i = 0; i < X86_PAGE_OR_DIR_SIZE; i++)
// {
//     // Only set the writable field to true.
//     kernel_table[i].present = true;
//     kernel_table[i].writable = true;
//     // ! should have '* MEM_ALIGNMENT_4K >> 12', but that's not necessary
//     kernel_table[i].mem_addr = i;
// }

// page_dir[0].present = true;
// page_dir[0].writable = true;
// page_dir[0].table_address = (u32) kernel_table >> 12;
