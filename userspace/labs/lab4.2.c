// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/kconfig.h>
#include <stdio.h>

static const bool vmm_debug_enabled = MOS_DEBUG_FEATURE(vmm);
static const bool cow_debug_enabled = MOS_DEBUG_FEATURE(cow);

int main(void)
{
    puts("Lab 4 Test Utility - Part 2");

    if (!vmm_debug_enabled)
        puts("Warning: VMM debug is disabled, you may not see any output from the VMM");

    if (!cow_debug_enabled)
        puts("Warning: COW debug is disabled, you may not see any output from the COW");

    puts("Here is a page fault on CoW stack");
    // somewhere in the middle of the stack
    const ptr_t stackptr = (ptr_t) __builtin_frame_address(0) - ((MOS_STACK_PAGES_USER / 2) * MOS_PAGE_SIZE);
    int *x = (int *) stackptr;
    *x = 0;

    puts("Some random page fault, this will cause a kernel panic");
    int *y = (int *) 0xdeadbeef;
    *y = 0;

    // unreachable
    if (syscall_get_pid() == 1)
        syscall_poweroff(false, MOS_FOURCC('G', 'B', 'y', 'e'));

    return 0;
}
