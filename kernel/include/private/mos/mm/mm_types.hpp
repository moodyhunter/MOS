// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/syslog/syslog.hpp"
#include "mos/types.hpp"

#include <mos/mm/mm_types.h>

enum VMFlag : unsigned int
{
    VM_NONE = 0,
    VM_READ = MEM_PERM_READ,   // 1 << 0
    VM_WRITE = MEM_PERM_WRITE, // 1 << 1
    VM_EXEC = MEM_PERM_EXEC,   // 1 << 2

    VM_USER = 1 << 3,
    VM_WRITE_THROUGH = 1 << 4,
    VM_CACHE_DISABLED = 1 << 5,
    VM_GLOBAL = 1 << 6,

    // composite flags (for convenience)
    VM_RW = VM_READ | VM_WRITE,
    VM_RX = VM_READ | VM_EXEC,
    VM_RWX = VM_READ | VM_WRITE | VM_EXEC,
    VM_USER_RW = VM_USER | VM_RW,
    VM_USER_RX = VM_USER | VM_RX,
    VM_USER_RO = VM_USER | VM_READ,
    VM_USER_RWX = VM_USER | VM_RWX,
};
MOS_ENUM_FLAGS(VMFlag, VMFlags);

inline mos::SyslogStream &operator<<(mos::SyslogStream &stream, VMFlags flags)
{
    return stream << (flags.test(VM_READ) ? 'r' : '-') << (flags.test(VM_WRITE) ? 'w' : '-') << (flags.test(VM_EXEC) ? 'x' : '-');
}
