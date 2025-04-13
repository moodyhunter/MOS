// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mos_global.h>
#include <mos/string.hpp>
#include <mos/vector.hpp>
#include <stddef.h>

void hexdump(const char *data, const size_t len);

mos::vector<mos::string> split_string(mos::string_view str, char delim);
