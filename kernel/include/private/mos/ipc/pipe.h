// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/io/io.h"
#include "mos/tasks/wait.h"

#include <mos/lib/structures/ring_buffer.h>

typedef struct
{
    u32 magic;
    io_t reader, writer; ///< readonly and writeonly io objects
    waitlist_t waitlist; ///< for both reader and writer, only one party can wait on the pipe at a time
    spinlock_t lock;     ///< protects the buffer_pos (and thus the buffer)
    void *buffers;
    size_t buffer_npages;
    ring_buffer_pos_t buffer_pos;
    bool closed;
} pipe_t;

pipe_t *pipe_create(size_t bufsize);
