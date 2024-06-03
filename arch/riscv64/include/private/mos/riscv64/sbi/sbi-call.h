// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>

void sbi_putchar(char ch);

size_t sbi_putstring(const char *str);
