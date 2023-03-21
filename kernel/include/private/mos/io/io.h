// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mm/mm_types.h>
#include <mos/types.h>

typedef struct _io_t io_t;

typedef enum
{
    IO_FILE = 1, // the io is a file
    IO_TERMINAL, // the io is a terminal
    IO_IPC,      // the io is an IPC channel
} io_type_t;

typedef enum
{
    IO_NONE = MEM_PERM_NONE,      // 0
    IO_READABLE = MEM_PERM_READ,  // 1 << 0
    IO_WRITABLE = MEM_PERM_WRITE, // 1 << 1
    // 1 << 2 is reserved for IO_EXECUTABLE
    IO_SEEKABLE = 1 << 3,
} io_flags_t;

typedef struct
{
    size_t (*read)(io_t *io, void *buf, size_t count);
    size_t (*write)(io_t *io, const void *buf, size_t count);
    void (*close)(io_t *io);
} io_op_t;

typedef struct _io_t
{
    bool closed;
    atomic_t refcount;
    io_flags_t flags;
    io_type_t type;
    const io_op_t *ops;
} io_t;

void io_init(io_t *io, io_type_t type, io_flags_t flags, const io_op_t *ops);

io_t *io_ref(io_t *io);
void io_unref(io_t *io);

size_t io_read(io_t *io, void *buf, size_t count);
size_t io_write(io_t *io, const void *buf, size_t count);
