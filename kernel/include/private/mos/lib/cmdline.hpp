// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.hpp>

bool cmdline_parse_inplace(char *inbuf, size_t length, size_t cmdline_max, size_t *cmdlines_count, const char **cmdlines);
const char **cmdline_parse(const char **inargv, char *inbuf, size_t length, size_t *out_count);

void string_unquote(char *str);
