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
