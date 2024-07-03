// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>

MOSAPI void init_start_cpiofs_server(fd_t notifier);

MOSAPI s64 strntoll(const char *str, char **endptr, int base, size_t n);
