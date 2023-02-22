// SDPX-License-Identifier: GPL-3.0-or-later
// This file defines the flags for mmap_anonymous and mmap_file system calls.

#pragma once

typedef enum
{
    MEM_PERM_NONE = 0,
    MEM_PERM_READ = 1 << 0,  // the memory is readable
    MEM_PERM_WRITE = 1 << 1, // the memory is writable
    MEM_PERM_EXEC = 1 << 2,  // the memory is executable
} mem_perm_t;

typedef enum
{
    MMAP_EXACT = 1 << 0,   // map the memory at the exact address specified
    MMAP_PRIVATE = 1 << 1, // the memory is private, and will be CoWed when forking
    MMAP_SHARED = 1 << 2,  // the memory is shared when forking
} mmap_flags_t;
