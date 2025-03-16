// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <iostream>

class DebugLogger
{
    static constexpr auto INIT_DEBUG = false;

  public:
    template<typename TArg>
    const DebugLogger &operator<<(TArg arg) const
    {
        if (INIT_DEBUG)
            std::cout << arg;
        return *this;
    }

    using ostream_manipulator = std::ostream &(*) (std::ostream &);

    const DebugLogger &operator<<(ostream_manipulator manip) const
    {
        if (INIT_DEBUG)
            manip(std::cout);
        return *this;
    }
};

inline const DebugLogger Debug;
