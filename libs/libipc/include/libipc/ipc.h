// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>

/**
 * @brief An IPC message.
 *
 */
typedef struct
{
    size_t size;
    char data[];
} ipc_msg_t;

/**
 * @brief Create a new IPC message.
 *
 * @param size The size of the message.
 * @return ipc_msg_t* The message.
 */
ipc_msg_t *ipc_msg_create(size_t size);

/**
 * @brief Destroy an IPC message.
 *
 * @param buffer The message.
 */
void ipc_msg_destroy(ipc_msg_t *buffer);

/**
 * @brief Read an IPC message.
 *
 * @param fd The file descriptor.
 * @return ipc_msg_t* The message.
 */
ipc_msg_t *ipc_read_msg(fd_t fd);

/**
 * @brief Write an IPC message.
 *
 * @param fd The file descriptor.
 * @param buffer The message.
 * @return true The message was written.
 * @return false The message was not written.
 */
bool ipc_write_msg(fd_t fd, ipc_msg_t *buffer);

size_t ipc_read_as_msg(fd_t fd, char *buffer, size_t buffer_size);
bool ipc_write_as_msg(fd_t fd, const char *data, size_t size);
