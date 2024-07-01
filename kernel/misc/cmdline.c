// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/platform/platform.h"

#include <mos/lib/cmdline.h>
#include <mos/misc/cmdline.h>
#include <mos/misc/kallsyms.h>
#include <mos/syslog/printk.h>
#include <mos_string.h>

static bool cmdline_is_truthy(const char *arg)
{
    return strcmp(arg, "true") == 0 || strcmp(arg, "1") == 0 || strcmp(arg, "yes") == 0 || strcmp(arg, "on") == 0;
}

static bool cmdline_is_falsy(const char *arg)
{
    return strcmp(arg, "false") == 0 || strcmp(arg, "0") == 0 || strcmp(arg, "no") == 0 || strcmp(arg, "off") == 0;
}

cmdline_option_t *cmdline_get_option(const char *option_name)
{
    for (u32 i = 0; i < platform_info->n_cmdlines; i++)
    {
        if (strcmp(platform_info->cmdlines[i].name, option_name) == 0)
        {
            return &platform_info->cmdlines[i];
        }
    }
    return NULL;
}

void mos_cmdline_init(const char *cmdline)
{
    // must be static so that it doesn't get freed
    static char cmdline_buf[MOS_PRINTK_BUFFER_SIZE];

    // first we copy EXTRA_CMDLINE to cmdline_buf
    size_t cmdline_len = 0;
    if (MOS_EXTRA_CMDLINE)
    {
        cmdline_len = strlen(MOS_EXTRA_CMDLINE);
        memcpy(cmdline_buf, MOS_EXTRA_CMDLINE, cmdline_len);
    }

    // then we append cmdline
    if (cmdline)
    {
        if (cmdline_len > 0)
        {
            cmdline_buf[cmdline_len] = ' ';
            cmdline_len++;
        }

        memcpy(cmdline_buf + cmdline_len, cmdline, strlen(cmdline));
        cmdline_len += strlen(cmdline);
    }

    cmdline_buf[cmdline_len] = '\0'; // ensure null terminator

    pr_dinfo2(setup, "cmdline: '%s'", cmdline_buf);

    const char *cmdlines_tmp[MOS_MAX_CMDLINE_COUNT] = { 0 };
    bool result = cmdline_parse_inplace(cmdline_buf, cmdline_len, MOS_MAX_CMDLINE_COUNT, &platform_info->n_cmdlines, cmdlines_tmp);
    if (!result)
        pr_warn("cmdline_parse: too many cmdlines");

    for (size_t i = 0; i < platform_info->n_cmdlines; i++)
    {
        pr_dinfo2(setup, "%s", cmdlines_tmp[i]);
        platform_info->cmdlines[i].name = cmdlines_tmp[i];

        // find the = sign
        char *equal_sign = strchr(cmdlines_tmp[i], '=');
        if (!equal_sign)
            continue;

        *equal_sign = '\0';
        platform_info->cmdlines[i].arg = equal_sign + 1;
    }
}

bool cmdline_string_truthiness(const char *arg, bool default_value)
{
    const char *func = mos_caller();
    func = func ? func : "";

    if (unlikely(!arg))
        return default_value;

    if (cmdline_is_truthy(arg))
        return true;
    else if (cmdline_is_falsy(arg))
        return false;
    else
        return default_value;
}
