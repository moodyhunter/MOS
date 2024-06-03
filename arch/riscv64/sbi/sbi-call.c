// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/riscv64/sbi/sbi-call.h"

#include <mos/types.h>
#include <mos_string.h>

typedef struct
{
    long error;
    long value;
} sbiret_t;

static sbiret_t sbi_ecall(int ext, int fid, ulong arg0, ulong arg1, ulong arg2, ulong arg3, ulong arg4, ulong arg5)
{
    sbiret_t ret;

    register ptr_t a0 __asm__("a0") = (ptr_t) (arg0);
    register ptr_t a1 __asm__("a1") = (ptr_t) (arg1);
    register ptr_t a2 __asm__("a2") = (ptr_t) (arg2);
    register ptr_t a3 __asm__("a3") = (ptr_t) (arg3);
    register ptr_t a4 __asm__("a4") = (ptr_t) (arg4);
    register ptr_t a5 __asm__("a5") = (ptr_t) (arg5);
    register ptr_t a6 __asm__("a6") = (ptr_t) (fid);
    register ptr_t a7 __asm__("a7") = (ptr_t) (ext);
    __asm__ volatile("ecall" : "+r"(a0), "+r"(a1) : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7) : "memory");
    ret.error = a0;
    ret.value = a1;

    return ret;
}

// Function Name	            SBI Version	FID	EID
// sbi_debug_console_write	    2	        0	0x4442434E
// sbi_debug_console_read	    2	        1	0x4442434E
// sbi_debug_console_write_byte	2	        2	0x4442434E

#define SBI_DEBUG_CONSOLE_EID       0x4442434E
#define SBI_EXT_0_1_CONSOLE_PUTCHAR 0x1

void sbi_putchar(char ch)
{
    sbi_ecall(SBI_EXT_0_1_CONSOLE_PUTCHAR, 0, ch, 0, 0, 0, 0, 0);
}

size_t sbi_putstring(const char *str)
{
    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++)
        sbi_putchar(str[i]);

    return len;
}
