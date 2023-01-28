// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

bool futex_wait(futex_word_t *futex, futex_word_t expected);
bool futex_wake(futex_word_t *lock, size_t num_to_wake);
