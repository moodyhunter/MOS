// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"

#include <mos/io/io_types.h>
#include <mos/mm/mm_types.h>
#include <mos/types.h>

typedef struct _io io_t;
typedef struct _vmap vmap_t; // forward declaration

typedef enum
{
    IO_NULL,    // null io port
    IO_FILE,    // a file
    IO_IPC,     // an IPC channel
    IO_CONSOLE, // a console
} io_type_t;

typedef enum
{
    IO_NONE = MEM_PERM_NONE,       // 0
    IO_READABLE = MEM_PERM_READ,   // 1 << 0
    IO_WRITABLE = MEM_PERM_WRITE,  // 1 << 1
    IO_EXECUTABLE = MEM_PERM_EXEC, // 1 << 2
    IO_SEEKABLE = 1 << 3,
    IO_MMAPABLE = 1 << 4,
} io_flags_t;

typedef struct
{
    size_t (*read)(io_t *io, void *buf, size_t count);
    size_t (*write)(io_t *io, const void *buf, size_t count);
    void (*close)(io_t *io);
    off_t (*seek)(io_t *io, off_t offset, io_seek_whence_t whence);
    bool (*mmap)(io_t *io, vmap_t *vmap, off_t offset);
} io_op_t;

typedef struct _io
{
    bool closed;
    atomic_t refcount;
    io_flags_t flags;
    io_type_t type;
    const io_op_t *ops;
} io_t;

extern io_t *const io_null;

void io_init(io_t *io, io_type_t type, io_flags_t flags, const io_op_t *ops);

io_t *io_ref(io_t *io);
io_t *io_unref(io_t *io);
__nodiscard bool io_valid(io_t *io);

size_t io_read(io_t *io, void *buf, size_t count);
size_t io_pread(io_t *io, void *buf, size_t count, off_t offset);
size_t io_write(io_t *io, const void *buf, size_t count);
off_t io_seek(io_t *io, off_t offset, io_seek_whence_t whence);
off_t io_tell(io_t *io);
bool io_mmap(io_t *io, vmap_t *vmap, off_t offset);
