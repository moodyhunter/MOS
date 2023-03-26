// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mm/mm_types.h>
#include <mos/types.h>

#define PATH_DELIM     '/'
#define PATH_DELIM_STR "/"
#define FD_CWD         (MOS_PROCESS_MAX_OPEN_FILES + 100)

#define DIR_ITERATOR_NTH_START 2 // 0 and 1 are '.' and '..'

typedef struct
{
    u64 accessed;
    u64 created;
    u64 modified;
} file_stat_time_t;

typedef enum
{
    FILE_TYPE_REGULAR,
    FILE_TYPE_DIRECTORY,
    FILE_TYPE_SYMLINK,
    FILE_TYPE_CHAR_DEVICE,
    FILE_TYPE_BLOCK_DEVICE,
    FILE_TYPE_NAMED_PIPE,
    FILE_TYPE_SOCKET,
    FILE_TYPE_UNKNOWN,
} file_type_t;

typedef enum
{
    OPEN_NONE = MEM_PERM_NONE,    // 0
    OPEN_READ = MEM_PERM_READ,    // 1 << 0
    OPEN_WRITE = MEM_PERM_WRITE,  // 1 << 1
    OPEN_EXECUTE = MEM_PERM_EXEC, // 1 << 2
    OPEN_NO_FOLLOW = 1 << 3,
    OPEN_CREATE = 1 << 4,
    OPEN_TRUNCATE = 1 << 5,
    OPEN_DIR = 1 << 6,
} open_flags;

typedef struct
{
    bool read, write, execute;
} __packed file_single_perm_t;

typedef struct
{
    file_single_perm_t owner;
    file_single_perm_t group;
    file_single_perm_t others;
} file_perm_t;

typedef struct
{
    file_type_t type;
    file_perm_t perm;
    uid_t uid;
    gid_t gid;
    bool sticky;
    bool suid;
    bool sgid;
    size_t size;
} file_stat_t;

typedef struct
{
    u64 ino;
    u64 next_offset; // sizeof(dir_entry_t) + name_len + 1
    file_type_t type;
    u32 name_len; // not including the null terminator
    char name[];
} dir_entry_t;

always_inline void file_format_perm(file_perm_t perms, char buf[10])
{
    buf[0] = perms.owner.read ? 'r' : '-';
    buf[1] = perms.owner.write ? 'w' : '-';
    buf[2] = perms.owner.execute ? 'x' : '-';
    buf[3] = perms.group.read ? 'r' : '-';
    buf[4] = perms.group.write ? 'w' : '-';
    buf[5] = perms.group.execute ? 'x' : '-';
    buf[6] = perms.others.read ? 'r' : '-';
    buf[7] = perms.others.write ? 'w' : '-';
    buf[8] = perms.others.execute ? 'x' : '-';
    buf[9] = '\0';
}
