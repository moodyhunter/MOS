// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/mm/mm_types.h>
#include <stdio.h>

int main(void)
{
    puts("Lab 4 Test Utility - Part 3");
    puts("Requesting a zeroed page by mmap_anonymous");
    u8 *vaddr = syscall_mmap_anonymous(0, MOS_PAGE_SIZE, MEM_PERM_READ | MEM_PERM_WRITE, MMAP_PRIVATE);

    if (vaddr == NULL)
    {
        puts("Error: mmap_anonymous returned NULL");
        return 1;
    }

    // verify that the page is zeroed
    puts("Verifying that the page is zeroed, this should not cause a page fault");
    for (size_t i = 0; i < MOS_PAGE_SIZE; i++)
    {
        if (vaddr[i] != 0)
        {
            puts("Error: mmap_anonymous did not zero the page");
            return 1;
        }
    }

    // write to the page
    printf("Writing to the page, this should not cause a page fault at the address %p\n", (void *) &vaddr[10]);
    vaddr[10] = 0x42;

    // verify the first byte is written
    if (vaddr[10] != 0x42)
    {
        puts("Error: mmap_anonymous did not write to the page");
        return 1;
    }

    puts("Success: mmap_anonymous works correctly");

    if (syscall_get_pid() == 1)
        syscall_poweroff(false, MOS_FOURCC('G', 'B', 'y', 'e'));

    return 0;
}
