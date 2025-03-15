// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <iostream>

class DebugLogger
{
    static inline bool debug = false;

  public:
    void SetDebug(bool value) const
    {
        debug = value;
    }

  public:
    template<typename... Args>
    const DebugLogger &operator<<(Args... args) const
    {
        if (debug)
            ((std::cout << args), ...) << std::endl;
        return *this;
    }

    const DebugLogger &operator<<(std::ostream &(*__pf)(std::ostream &) ) const
    {
        // _GLIBCXX_RESOLVE_LIB_DEFECTS
        // DR 60. What is a formatted input function?
        // The inserters for manipulators are *not* formatted output functions.
        __pf(std::cout);
        return *this;
    }
};

inline const DebugLogger Logger;
