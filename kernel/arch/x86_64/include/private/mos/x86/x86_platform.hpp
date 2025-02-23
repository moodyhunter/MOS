// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.hpp"

#include <mos/type_utils.hpp>

#define X86_BIOS_MEMREGION_PADDR 0xf0000
#define BIOS_MEMREGION_SIZE      0x10000

#define X86_EBDA_MEMREGION_PADDR 0x80000
#define EBDA_MEMREGION_SIZE      0x20000

#define X86_VIDEO_DEVICE_PADDR 0xb8000

#define MOS_SYSCALL_INTR 0x88

struct platform_regs_t : mos::NamedType<"Platform.Registers">
{
    platform_regs_t()
    {
        memzero(this, sizeof(*this));
    }

    platform_regs_t(const platform_regs_t *regs)
    {
        *this = *regs;
    }

    reg_t r15, r14, r13, r12, r11, r10, r9, r8;
    reg_t di, si, bp, dx, cx, bx, ax;
    reg_t interrupt_number, error_code;
    // iret params
    reg_t ip, cs;
    reg_t eflags;
    reg_t sp, ss;
} __packed;

MOS_STATIC_ASSERT(sizeof(platform_regs_t) == 176, "platform_regs_t has incorrect size");

extern mos_platform_info_t x86_platform;
void x86_dump_stack_at(ptr_t this_frame, bool can_access_vmaps);

void x86_setup_lapic_timer();
