// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef struct _io_t io_t;

typedef enum
{
    IO_TYPE_NONE = 0,
    IO_READABLE = 1 << 0,
    IO_WRITABLE = 1 << 1,
    IO_SEEKABLE = 1 << 2,
} io_flags_t;

typedef struct
{
    // ref hooks
    void (*before_ref)(io_t *io);
    void (*after_unref)(io_t *io);

    size_t (*read)(io_t *io, void *buf, size_t count);
    size_t (*write)(io_t *io, const void *buf, size_t count);
    void (*close)(io_t *io);
} io_op_t;

typedef struct _io_t
{
    bool closed;
    atomic_t refcount;
    io_flags_t flags;
    size_t size;
    const io_op_t *ops;
} io_t;

void io_init(io_t *io, io_flags_t flags, size_t size, const io_op_t *ops);

void io_ref(io_t *io);
void io_unref(io_t *io);

size_t io_read(io_t *io, void *buf, size_t count);
size_t io_write(io_t *io, const void *buf, size_t count);
