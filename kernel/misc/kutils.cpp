// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/misc/kutils.hpp"

#include "mos/syslog/printk.hpp"

#include <bits/c++config.h>
#include <mos/string.hpp>

static const int HEXDUMP_COLS = 16;

void hexdump(const char *data, const size_t len)
{
    if (len)
        pr_info("  " PTR_FMT ": ", (ptr_t) data);

    for (size_t i = 0; i < len; i++)
    {
        pr_cont("%02hhx ", (char) data[i]);
        if ((i + 1) % HEXDUMP_COLS == 0)
        {
            for (size_t j = i - (HEXDUMP_COLS - 1); j <= i; j++)
            {
                const char c = data[j];
                pr_cont("%c", c >= 32 && c <= 126 ? c : '.');
            }

            if (i + 1 < len)
                pr_info("  " PTR_FMT ": ", (ptr_t) (data + i + 1));
        }
    }

    if (len % HEXDUMP_COLS != 0)
    {
        const size_t spaces = (HEXDUMP_COLS - (len % HEXDUMP_COLS)) * 3;
        pr_cont("%*c", (int) spaces, ' ');
        for (size_t i = len - (len % HEXDUMP_COLS); i < len; i++)
        {
            const char c = data[i];
            pr_cont("%c", c >= 32 && c <= 126 ? c : '.');
        }
    }

    pr_info("");
}

mos::vector<mos::string> split_string(mos::string_view str, char delim)
{
    mos::vector<mos::string> result;
    size_t start = 0;
    size_t end = str.find(delim);

    const auto add_substr = [&](mos::string_view str, size_t start, size_t end)
    {
        if (const auto substr = str.substr(start, end - start); !substr.empty())
            result.push_back(mos::string(substr));
    };
    while (end != mos::string::npos)
    {
        add_substr(str, start, end);
        start = end + 1;
        end = str.find(delim, start);
    }

    add_substr(str, start, end);
    return result;
}

int days_from_civil(int y, unsigned m, unsigned d) noexcept
{
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);            // [0, 399]
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1; // [0, 365]
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;           // [0, 146096]
    return era * 146097 + static_cast<int>(doe) - 719468;
}
