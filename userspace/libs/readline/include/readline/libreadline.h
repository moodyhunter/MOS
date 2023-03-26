// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>

/**
 * @brief Read a line from stdin
 *
 * @param prompt Prompt to display
 * @return char*
 */
char *readline(const char *prompt);

/**
 * @brief Read a line from a file descriptor
 *
 * @param fd File descriptor to read from
 * @return char*
 */
char *get_line(fd_t fd);
