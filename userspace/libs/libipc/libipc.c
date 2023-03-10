// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/string.h"
#include "libipc/ipc.h"

ipc_msg_t *ipc_msg_create(size_t size)
{
    ipc_msg_t *buffer = malloc(sizeof(ipc_msg_t) + size);
    memzero(buffer, sizeof(ipc_msg_t) + size);
    buffer->size = size;
    return buffer;
}

void ipc_msg_destroy(ipc_msg_t *buffer)
{
    free(buffer);
}

ipc_msg_t *ipc_read_msg(fd_t fd)
{
    size_t size = 0;
    size_t read_size = syscall_io_read(fd, &size, sizeof(size), 0);
    if (read_size != sizeof(size))
    {
        dprintf(stderr, "failed to read size from ipc channel\n");
        return NULL;
    }

    ipc_msg_t *buffer = ipc_msg_create(size);
    read_size = syscall_io_read(fd, buffer->data, buffer->size, 0);
    if (read_size != size)
    {
        dprintf(stderr, "failed to read data from ipc channel\n");
        ipc_msg_destroy(buffer);
        return NULL;
    }

    return buffer;
}

bool ipc_write_msg(fd_t fd, ipc_msg_t *buffer)
{
    size_t written = syscall_io_write(fd, &buffer->size, sizeof(buffer->size), 0);
    if (written != sizeof(buffer->size))
    {
        dprintf(stderr, "failed to write size to ipc channel\n");
        return false;
    }

    written = syscall_io_write(fd, buffer->data, buffer->size, 0);
    if (written != buffer->size)
    {
        dprintf(stderr, "failed to write data to ipc channel\n");
        return false;
    }

    return true;
}

bool ipc_write_as_msg(fd_t fd, const char *data, size_t size)
{
    size_t w = 0;
    w = syscall_io_write(fd, &size, sizeof(size), 0);
    if (unlikely(w != sizeof(size)))
    {
        dprintf(stderr, "failed to write size to ipc channel\n");
        return false;
    }
    w = syscall_io_write(fd, data, size, 0);
    if (unlikely(w != size))
    {
        dprintf(stderr, "failed to write data to ipc channel\n");
        return false;
    }

    return true;
}

size_t ipc_read_as_msg(fd_t fd, char *buffer, size_t buffer_size)
{
    size_t r = 0;
    size_t data_size = 0;
    r = syscall_io_read(fd, &data_size, sizeof(size_t), 0);
    if (unlikely(r != sizeof(size_t)))
    {
        dprintf(stderr, "failed to read size from ipc channel\n");
        return 0;
    }

    if (unlikely(data_size > buffer_size))
    {
        dprintf(stderr, "buffer too small\n");
        return 0;
    }

    r = syscall_io_read(fd, buffer, buffer_size, 0);
    if (unlikely(r != data_size))
    {
        dprintf(stderr, "failed to read data from ipc channel\n");
        return 0;
    }
    return data_size;
}
