// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/cmdline.h>
#include <mos/mm/kmalloc.h>
#include <mos/printk.h>
#include <string.h>

cmdline_t *mos_cmdline = NULL;

cmdline_t *cmdline_create(const char *cmdline)
{
    cmdline_t *cmd = kzalloc(sizeof(cmdline_t));

    while (*cmdline)
    {
        if (*cmdline == ' ')
            cmdline++;

        cmd->options = krealloc(cmd->options, sizeof(cmdline_option_t *) * (cmd->options_count + 1));
        cmdline_option_t *opt = cmd->options[cmd->options_count++] = kzalloc(sizeof(cmdline_option_t));

        // an arg name ends with an '=' (has parameters) or a space
        {
            const char *const start = cmdline;
            while (*cmdline && !((*cmdline) == ' ' || (*cmdline) == '='))
                cmdline++;
            opt->name = duplicate_string(start, cmdline - start);
        }

        // the option has more arguments
        if (*cmdline && *cmdline++ != '=')
            continue;

        while (*cmdline && *cmdline != ' ')
        {
            opt->argv = krealloc(opt->argv, sizeof(const char *) * (opt->argc + 1));

            // an option ends with a space or a comma
            const char *const start = cmdline;
            while (*cmdline && *cmdline != ' ' && *cmdline != ',')
                cmdline++;

            opt->argv[opt->argc++] = duplicate_string(start, cmdline - start);

            MOS_ASSERT(*cmdline == ' ' || *cmdline == ',' || *cmdline == '\0');

            // we have another argument
            if (*cmdline == ',')
                cmdline++;

            // we have another option
            if (*cmdline == ' ')
            {
                cmdline++;
                break;
            }
        }
    }

    return cmd;
}

static void cmdline_free_option(cmdline_option_t *opt)
{
    kfree(opt->name);
    for (u32 i = 0; i < opt->argc; i++)
        kfree(opt->argv[i]);
    if (opt->argc > 0)
        kfree(opt->argv);
    kfree(opt);
}

bool cmdline_remove_option(cmdline_t *cmdline, const char *arg)
{
    for (u32 i = 0; i < cmdline->options_count; i++)
    {
        if (strcmp(cmdline->options[i]->name, arg) == 0)
        {
            cmdline_free_option(cmdline->options[i]);
            cmdline->options_count--;

            for (u32 j = i; j < cmdline->options_count; j++)
                cmdline->options[j] = cmdline->options[j + 1]; // shift the array to the left by one

            cmdline->options = krealloc(cmdline->options, sizeof(cmdline_option_t *) * cmdline->options_count);
            return true;
        }
    }
    return false;
}

void cmdline_destroy(cmdline_t *cmdline)
{
    for (u32 i = 0; i < cmdline->options_count; i++)
        cmdline_free_option(cmdline->options[i]);

    kfree(cmdline->options);
    kfree(cmdline);
}

static bool cmdline_is_truthy(const char *arg)
{
    return strcmp(arg, "true") == 0 || strcmp(arg, "1") == 0 || strcmp(arg, "yes") == 0 || strcmp(arg, "on") == 0;
}

static bool cmdline_is_falsy(const char *arg)
{
    return strcmp(arg, "false") == 0 || strcmp(arg, "0") == 0 || strcmp(arg, "no") == 0 || strcmp(arg, "off") == 0;
}

bool cmdline_arg_get_bool_impl(const char *func, int argc, const char **argv, bool default_value)
{
    func = func ? func : "";
    if (argc == 0)
        return default_value;
    else if (argc > 1)
        pr_warn("%s: too many arguments (%d), only the first one will be used", func, argc);

    if (cmdline_is_truthy(argv[0]))
        return true;
    else if (cmdline_is_falsy(argv[0]))
        return false;
    else
        pr_warn("%s: invalid argument '%s', assuming %s", func, argv[0], default_value ? "true" : "false");

    return default_value;
}
