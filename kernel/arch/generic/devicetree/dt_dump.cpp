// SPDX-License-Identifier: GPL-3.0-or-later

#include "libfdt++.hpp"
#include "mos/syslog/printk.h"

#include <mos_string.h>

#define INDENT "    "

static void print_property_value(const dt_property &prop, size_t indent_len = 0)
{
    const char *string = prop.get_string();
    const int len = prop.len();

    // a printable string shouldn't start with '\0' and should end with '\0'
    bool printable = (string[0] != '\0') && (string[len - 1] == '\0');

    if (printable)
    {
        for (int i = 0; i < len; ++i)
        {
            const char c = string[i];
            if (!(c >= 32 && c <= 126) && (c != '\0'))
            {
                printable = false;
                break;
            }
        }
    }

    if (printable)
    {
        pr_cont("\"");
        for (int i = 0; i < len - 1; ++i)
        {
            char c = string[i];
            if (c == '\0')
                c = '|';
            pr_cont("%c", c);
        }
        pr_cont("\"");
    }
    else if (prop.len() == 4)
        pr_cont("0x%x", prop.get_u32());
    else if (prop.len() == 8)
        pr_cont("0x%llx", prop.get_u64());
    else
    {
        if (strcmp(prop.get_name(), "reg") == 0)
        {
            const dt_reg regs = prop;
            if (!regs.verify_validity())
            {
                pr_cont("<invalid reg>:");
                goto do_dump;
            }

            for (const auto &[base, size] : regs)
                pr_cont("(" PTR_VLFMT ", %lu)", base, size);
        }
        else
        {
        do_dump:
            const char *data = prop.get_string();
            const size_t len = prop.len();

            for (size_t i = 0; i < len; ++i)
            {
                if (i % 16 == 0)
                {
                    // print string
                    for (size_t j = i - 16; j <= i; j++)
                    {
                        const char c = data[j];
                        pr_cont("%c", c >= 32 && c <= 126 ? c : '.');
                    }

                    if (i != 0)
                    {
                        pr_cont("\n");
                        for (size_t j = 0; j < indent_len; ++j)
                            pr_cont(" ");
                    }
                }

                pr_cont("%02x ", data[i]);
            }

            if (len % 16 != 0)
            {
                size_t spaces = (16 - (len % 16)) * 3;

                if (len < 16)
                    spaces = 3;

                pr_cont("%*c", (int) spaces, ' ');
                for (size_t i = len - (len % 16); i < len; i++)
                {
                    const char c = data[i];
                    pr_cont("%c", c >= 32 && c <= 126 ? c : '.');
                }
            }
        }
    }

    pr_cont("\n");
}

void dump_fdt_node(const dt_node &node, int depth = 0)
{
    if (depth == 0)
        pr_info();

    for (int i = 0; i < depth; ++i)
        pr_cont(INDENT);

    const auto name = node.get_name();
    if (strlen(name) == 0)
    {
        MOS_ASSERT(depth == 0); // root node should have an empty name
        pr_cont("/ {\n");
    }
    else
        pr_cont("%s {\n", name);

    // Print properties
    for (const auto prop : node.properties())
    {
        for (int i = 0; i < depth + 1; ++i)
            pr_cont(INDENT);

        pr_cont("%s", prop.get_name());
        if (prop.len())
        {
            pr_cont(" = ");

            const size_t idenet_len = strlen(prop.get_name()) + 3 + (depth + 1) * strlen(INDENT);
            print_property_value(prop, idenet_len);
        }
        else
        {
            pr_cont("\n");
        }
    }

    // Print subnodes
    for (const auto child : node)
        dump_fdt_node(child, depth + 1);

    for (int i = 0; i < depth; ++i)
        pr_cont(INDENT);
    pr_cont("}\n");
}
