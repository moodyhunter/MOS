// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/x86/x86_platform.h"

typedef struct
{
    u32 link;
    u32 esp0, ss0;
    u32 esp1, ss1;
    u32 esp2, ss2;

    u32 cr3;
    u32 eip;
    u32 eflags;

    u32 eax, ecx, edx, ebx, esp, ebp, esi, edi;
    u32 es, cs, ss, ds, fs, gs;

    u32 ldtr;
    u16 trap;
    u16 iomap;
} __attr_packed tss32_t;

static_assert(sizeof(tss32_t) == 104, "tss32_t is not 104 bytes");
