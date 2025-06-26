// SPDX-License-Identifier: GPL-3.0-or-later

#include "header.hpp"
#include "mos/syslog/printk.hpp"

void kmodtest_header_test(const mos::string &str)
{
    pr_emph("");
    pr_emph("");
    pr_emph("=====================================");
    pr_emph("  MOS Kernel Module: %s", str.c_str());
    pr_emph("=====================================");
    pr_emph("");
    pr_emph("\n");
}
