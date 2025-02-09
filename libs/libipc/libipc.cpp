// SPDX-License-Identifier: GPL-3.0-or-later

#include "libipc/ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifdef __MOS_KERNEL__
#include "mos/assert.hpp"
#include "mos/io/io.hpp"

#include <mos_stdlib.hpp>
#define do_read(fd, buffer, size)  io_read(fd, buffer, size)
#define do_write(fd, buffer, size) io_write(fd, buffer, size)
#define do_warn(fmt, ...)          mos_warn(fmt, ##__VA_ARGS__)
#else
#include <unistd.h>
#define do_read(fd, buffer, size)  read(fd, buffer, size)
#define do_write(fd, buffer, size) write(fd, buffer, size)
#define do_warn(fmt, ...)          fprintf(stderr, fmt __VA_OPT__(, ) __VA_ARGS__)
#endif

MOS_STATIC_ASSERT(sizeof(size_t) == sizeof(uint64_t), "size_t must be 64 bits");

ipc_msg_t *ipc_msg_create(size_t size)
{
    ipc_msg_t *buffer = (ipc_msg_t *) malloc(sizeof(ipc_msg_t) + size);
    buffer->size = size;
    return buffer;
}

void ipc_msg_destroy(ipc_msg_t *buffer)
{
    free(buffer);
}

ipc_msg_t *ipc_read_msg(ipcfd_t fd)
{
    size_t size = 0;
    size_t read_size = do_read(fd, &size, sizeof(size));

    if (read_size == 0)
    {
        // EOF
        return NULL;
    }

    if (read_size != sizeof(size))
    {
        do_warn("failed to read size from ipc channel");
        return NULL;
    }

    ipc_msg_t *buffer = ipc_msg_create(size);
    read_size = do_read(fd, buffer->data, buffer->size);
    if (read_size != size)
    {
        do_warn("failed to read data from ipc channel");
        ipc_msg_destroy(buffer);
        return NULL;
    }

    return buffer;
}

bool ipc_write_msg(ipcfd_t fd, ipc_msg_t *buffer)
{
    size_t written = do_write(fd, &buffer->size, sizeof(buffer->size));
    if (written != sizeof(buffer->size))
    {
        do_warn("failed to write size to ipc channel");
        return false;
    }

    written = do_write(fd, buffer->data, buffer->size);
    if (written != buffer->size)
    {
        do_warn("failed to write data to ipc channel");
        return false;
    }

    return true;
}

bool ipc_write_as_msg(ipcfd_t fd, const char *data, size_t size)
{
    size_t w = 0;
    w = do_write(fd, &size, sizeof(size));
    if (unlikely(w != sizeof(size)))
    {
        do_warn("failed to write size to ipc channel");
        return false;
    }
    w = do_write(fd, data, size);
    if (unlikely(w != size))
    {
        do_warn("failed to write data to ipc channel");
        return false;
    }

    return true;
}

size_t ipc_read_as_msg(ipcfd_t fd, char *buffer, size_t buffer_size)
{
    size_t size = 0;
    size_t data_size = 0;
    size = do_read(fd, &data_size, sizeof(size_t));
    if (unlikely(size != sizeof(size_t)))
    {
        do_warn("failed to read size from ipc channel");
        return 0;
    }

    if (unlikely(data_size > buffer_size))
    {
        do_warn("buffer too small");
        return 0;
    }

    size = do_read(fd, buffer, buffer_size);
    if (unlikely(size != data_size))
    {
        do_warn("failed to read data from ipc channel");
        return 0;
    }
    return data_size;
}
