// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef enum
{
    IO_TYPE_FILE,
    IO_TYPE_SOCKET,
} io_type_t;

typedef enum
{
    IO_READABLE = 1 << 0,
    IO_WRITABLE = 1 << 1,
    IO_SEEKABLE = 1 << 2,
} io_flags_t;

typedef struct _io_t
{
    atomic_t refcount;
    io_type_t type;
    io_flags_t flags;
    void *data_ptr;
} io_t;
