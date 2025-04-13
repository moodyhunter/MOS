// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/string.hpp>
#include <mos/types.hpp>
#include <mos/vector.hpp>

bool cmdline_parse_inplace(char *inbuf, size_t length, size_t cmdline_max, size_t *cmdlines_count, const char **cmdlines);
const char **cmdline_parse(char *inbuf, size_t length, const char **inargv, size_t *out_count);
mos::vector<mos::string> cmdline_parse_vector(char *inbuf, size_t length);

void string_unquote(char *str);
