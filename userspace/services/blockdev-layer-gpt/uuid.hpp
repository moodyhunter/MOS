// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <iomanip>
#include <ostream>
#include <string.h>

struct UUID
{
    explicit UUID(const unsigned char full[16]) : full{}
    {
        memcpy(this->full, full, 16);
    }

    friend std::ostream &operator<<(std::ostream &os, const UUID &guid)
    {
        const auto flags = os.flags();
        os << std::uppercase << std::hex;

        for (size_t i : { 3, 2, 1, 0, /**/ 5, 4, /**/ 7, 6, /**/ 8, 9, /**/ 10, 11, 12, 13, 14, 15 }) // little endian
        {
            if (i == 5 || i == 7 || i == 8 || i == 10)
                os << '-';

            os << std::setw(2) << std::setfill('0') << (int) guid.full[i];
        }

        return os << std::resetiosflags(flags);
    }

  private:
    unsigned char full[16];
};
