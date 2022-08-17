// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/cmdline.h"

#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"

#define copy_string(dst, src, len) dst = kmalloc(len + 1), strncpy(dst, src, len)

cmdline_t *parse_cmdline(const char *cmdline)
{
    cmdline_t *cmd = kmalloc(sizeof(cmdline_t));
    cmd->options_count = 0;

    while (*cmdline)
    {
        cmdline_option_t *option = kmalloc(sizeof(cmdline_option_t));
        option->parameters_count = 0;
        cmd->options[cmd->options_count++] = option;

        // an option name ends with an '=' (has parameters) or a space
        {
            const char *start = cmdline;
            while (*cmdline && !((*cmdline) == ' ' || (*cmdline) == '='))
                cmdline++;
            copy_string(option->name, start, cmdline - start);
        }

        // the option has parameters
        if (*cmdline++ != '=')
            continue;

        while (*cmdline && *cmdline != ' ')
        {
            cmdline_parameter_t *parameter = kmalloc(sizeof(cmdline_parameter_t));
            option->parameters[option->parameters_count++] = parameter;

            if (strncmp(cmdline, "true", 4) == 0)
            {
                parameter->param_type = CMDLINE_PARAM_TYPE_BOOL;
                parameter->val.boolean = true;
                cmdline += 4;
            }
            else if (strncmp(cmdline, "false", 5) == 0)
            {
                parameter->param_type = CMDLINE_PARAM_TYPE_BOOL;
                parameter->val.boolean = false;
                cmdline += 5;
            }
            else
            {
                // an option ends with a space or a comma
                const char *param_start = cmdline;
                while (*cmdline && *cmdline != ' ' && *cmdline != ',')
                    cmdline++;

                copy_string(parameter->val.string, param_start, cmdline - param_start);
                parameter->param_type = CMDLINE_PARAM_TYPE_STRING;
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

cmdline_option_t *cmdline_get_option(cmdline_t *cmd, const char *option_name)
{
    for (u32 i = 0; i < cmd->options_count; i++)
    {
        if (strcmp(cmd->options[i]->name, option_name) == 0)
            return cmd->options[i];
    }
    return NULL;
}
