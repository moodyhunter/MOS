// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/io/io.hpp"

PtrResult<io_t> memfd_create(const char *name);
