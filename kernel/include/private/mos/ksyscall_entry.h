// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <mos/types.h>

reg_t ksyscall_enter(reg_t num, reg_t arg0, reg_t arg1, reg_t arg2, reg_t arg3, reg_t arg4, reg_t arg5);
