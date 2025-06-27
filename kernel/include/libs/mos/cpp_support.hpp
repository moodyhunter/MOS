// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace mos
{
    [[noreturn]] void __raise_bad_ptrresult_value(int errorCode);
    [[noreturn]] void __raise_null_pointer_exception();
    [[noreturn]] void __raise_invalid_argument_exception(const char *reason);
}; // namespace mos
