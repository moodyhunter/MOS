// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mm/mm_types.h>
#include <mos/mos_global.h>
#include <mos/types.h>

#define PATH_DELIM     '/'
#define PATH_DELIM_STR "/"
#define FD_CWD         (MOS_PROCESS_MAX_OPEN_FILES + 100)

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
    OPEN_APPEND = 1 << 7,
    OPEN_EXCLUSIVE = 1 << 8,
} open_flags;

typedef enum
{
    FSTATAT_NONE = 0,
    FSTATAT_NOFOLLOW = 1 << 1, // lstat, operates on the link itself
    FSTATAT_FILE = 1 << 2,     // the fd is a file, not a directory
} fstatat_flags;

typedef enum
{
    FD_FLAGS_NONE = 0,
    FD_FLAGS_CLOEXEC = 1 << 0,
} fd_flags_t;

typedef u16 file_perm_t;

#define PERM_OWNER 0x1C0 // 111 000 000
#define PERM_GROUP 0x38  // 000 111 000
#define PERM_OTHER 0x7   // 000 000 111
#define PERM_READ  0x124 // 100 100 100
#define PERM_WRITE 0x92  // 010 010 010
#define PERM_EXEC  0x49  // 001 001 001

#define PERM_MASK 0777

typedef struct
{
    u64 ino;
    file_type_t type;
    file_perm_t perm;
    size_t size;
    uid_t uid;
    gid_t gid;
    bool sticky;
    bool suid;
    bool sgid;
    ssize_t nlinks;
    u64 accessed;
    u64 created;
    u64 modified;
} file_stat_t;

should_inline void file_format_perm(file_perm_t perms, char buf[10])
{
    buf[0] = perms & (PERM_READ & PERM_OWNER) ? 'r' : '-';
    buf[1] = perms & (PERM_WRITE & PERM_OWNER) ? 'w' : '-';
    buf[2] = perms & (PERM_EXEC & PERM_OWNER) ? 'x' : '-';

    buf[3] = perms & (PERM_READ & PERM_GROUP) ? 'r' : '-';
    buf[4] = perms & (PERM_WRITE & PERM_GROUP) ? 'w' : '-';
    buf[5] = perms & (PERM_EXEC & PERM_GROUP) ? 'x' : '-';

    buf[6] = perms & (PERM_READ & PERM_OTHER) ? 'r' : '-';
    buf[7] = perms & (PERM_WRITE & PERM_OTHER) ? 'w' : '-';
    buf[8] = perms & (PERM_EXEC & PERM_OTHER) ? 'x' : '-';
    buf[9] = '\0';
}
