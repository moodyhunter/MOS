// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/cmdline.h>
#include <mos/kallsyms.h>
#include <mos/lib/cmdline.h>
#include <mos/mm/kmalloc.h>
#include <mos/printk.h>
#include <string.h>

size_t mos_cmdlines_count = 0;
cmdline_option_t mos_cmdlines[MOS_MAX_CMDLINE_COUNT] = { 0 };

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
    for (u32 i = 0; i < mos_cmdlines_count; i++)
    {
        if (strcmp(mos_cmdlines[i].name, option_name) == 0)
        {
            return &mos_cmdlines[i];
        }
    }
    return NULL;
}

void mos_cmdline_parse(const char *cmdline)
{
    // must be static so that it doesn't get freed
    static char cmdline_buf[PRINTK_BUFFER_SIZE] = { 0 };

    if (!cmdline)
        return;

    const size_t cmdline_length = strlen(cmdline);
    memcpy(cmdline_buf, cmdline, cmdline_length);
    cmdline_buf[cmdline_length] = '\0'; // ensure null terminator

    mos_debug(setup, "cmdline: %s", cmdline_buf);

    const char *cmdlines_tmp[MOS_MAX_CMDLINE_COUNT] = { 0 };
    bool result = cmdline_parse_inplace(cmdline_buf, cmdline_length, MOS_MAX_CMDLINE_COUNT, &mos_cmdlines_count, cmdlines_tmp);
    if (!result)
        pr_warn("cmdline_parse: too many cmdlines");

    for (size_t i = 0; i < mos_cmdlines_count; i++)
    {
        mos_debug(setup, "cmdline: %s", cmdlines_tmp[i]);
        mos_cmdlines[i].name = cmdlines_tmp[i];

        // find the = sign
        char *equal_sign = strchr(cmdlines_tmp[i], '=');
        if (!equal_sign)
            continue;

        *equal_sign = '\0';
        mos_cmdlines[i].arg = equal_sign + 1;
    }
}

bool cmdline_string_truthiness(const char *arg, bool default_value)
{
    const char *func = mos_caller;
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
