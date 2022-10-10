// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/cmdline.h"

#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"

cmdline_t *mos_cmdline = NULL;

cmdline_t *mos_cmdline_create(const char *cmdline)
{
    cmdline_t *cmd = kmalloc(sizeof(cmdline_t));
    cmd->args_count = 0;
    cmd->arguments = NULL;

    while (*cmdline)
    {
        if (*cmdline == ' ')
            cmdline++;
        cmdline_arg_t *arg = kmalloc(sizeof(cmdline_arg_t));
        arg->params = NULL;
        arg->params_count = 0;
        cmd->arguments = krealloc(cmd->arguments, sizeof(cmdline_arg_t *) * (cmd->args_count + 1));
        cmd->arguments[cmd->args_count] = arg;
        cmd->args_count++;

        // an arg name ends with an '=' (has parameters) or a space
        {
            const char *start = cmdline;
            while (*cmdline && !((*cmdline) == ' ' || (*cmdline) == '='))
                cmdline++;
            arg->arg_name = duplicate_string_n(start, cmdline - start);
        }

        // the option has parameters
        if (*cmdline && *cmdline++ != '=')
            continue;

        while (*cmdline && *cmdline != ' ')
        {
            cmdline_param_t *param = kmalloc(sizeof(cmdline_param_t));
            arg->params = krealloc(arg->params, sizeof(cmdline_param_t *) * (arg->params_count + 1));
            arg->params[arg->params_count] = param;
            arg->params_count++;

            if (strncmp(cmdline, "true", 4) == 0)
            {
                param->param_type = CMDLINE_PARAM_TYPE_BOOL;
                param->val.boolean = true;
                cmdline += 4;
            }
            else if (strncmp(cmdline, "false", 5) == 0)
            {
                param->param_type = CMDLINE_PARAM_TYPE_BOOL;
                param->val.boolean = false;
                cmdline += 5;
            }
            else
            {
                param->param_type = CMDLINE_PARAM_TYPE_STRING;
                // an option ends with a space or a comma
                const char *param_start = cmdline;
                while (*cmdline && *cmdline != ' ' && *cmdline != ',')
                    cmdline++;

                param->val.string = duplicate_string_n(param_start, cmdline - param_start);
            }

            MOS_ASSERT(*cmdline == ' ' || *cmdline == ',' || *cmdline == '\0');

            // we have another option
            if (*cmdline == ',')
                cmdline++;

            // we have no other options, continue with another parameter
            if (*cmdline == ' ')
            {
                cmdline++;
                break;
            }
        }
    }

    return cmd;
}

void mos_cmdline_destroy(cmdline_t *cmdline)
{
    for (u32 i = 0; i < cmdline->args_count; i++)
    {
        cmdline_arg_t *arg = cmdline->arguments[i];
        kfree(arg->arg_name);
        for (u32 j = 0; j < arg->params_count; j++)
        {
            cmdline_param_t *param = arg->params[j];
            if (param->param_type == CMDLINE_PARAM_TYPE_STRING)
                kfree(param->val.string);
            kfree(param);
        }
        if (arg->params_count)
            kfree(arg->params);
        kfree(arg);
    }
    kfree(cmdline->arguments);
    kfree(cmdline);
}

cmdline_arg_t *mos_cmdline_get_arg(const char *option_name)
{
    MOS_ASSERT(mos_cmdline);
    for (u32 i = 0; i < mos_cmdline->args_count; i++)
    {
        if (strcmp(mos_cmdline->arguments[i]->arg_name, option_name) == 0)
            return mos_cmdline->arguments[i];
    }
    return NULL;
}
