// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/cmdline.h"
#include "test_engine.h"

MOS_TEST_CASE(simple_cmdline)
{
    const char *single = "single";
    cmdline_t *cmd = mos_cmdline_create(single);
    MOS_TEST_CHECK(cmd != NULL, true);
    MOS_TEST_CHECK(cmd->args_count, 1);
    MOS_TEST_CHECK_STRING(cmd->arguments[0]->arg_name, "single");
    MOS_TEST_CHECK(cmd->arguments[0]->params_count, 0);
    mos_cmdline_destroy(cmd);
}

MOS_TEST_CASE(two_arguments)
{
    const char *two = "one two";
    cmdline_t *cmd = mos_cmdline_create(two);
    MOS_TEST_CHECK(cmd != NULL, true);
    MOS_TEST_CHECK(cmd->args_count, 2);
    MOS_TEST_CHECK_STRING(cmd->arguments[0]->arg_name, "one");
    MOS_TEST_CHECK(cmd->arguments[0]->params_count, 0);
    MOS_TEST_CHECK_STRING(cmd->arguments[1]->arg_name, "two");
    MOS_TEST_CHECK(cmd->arguments[1]->params_count, 0);
    mos_cmdline_destroy(cmd);
}

MOS_TEST_CASE(many_arguments)
{
    const char *many = "one two three four five six seven eight nine ten";
    cmdline_t *cmd = mos_cmdline_create(many);
    MOS_TEST_CHECK(cmd != NULL, true);
    MOS_TEST_CHECK(cmd->args_count, 10);
    MOS_TEST_CHECK_STRING(cmd->arguments[0]->arg_name, "one");
    MOS_TEST_CHECK(cmd->arguments[0]->params_count, 0);
    MOS_TEST_CHECK_STRING(cmd->arguments[1]->arg_name, "two");
    MOS_TEST_CHECK(cmd->arguments[1]->params_count, 0);
    MOS_TEST_CHECK_STRING(cmd->arguments[2]->arg_name, "three");
    MOS_TEST_CHECK(cmd->arguments[2]->params_count, 0);
    MOS_TEST_CHECK_STRING(cmd->arguments[3]->arg_name, "four");
    MOS_TEST_CHECK(cmd->arguments[3]->params_count, 0);
    MOS_TEST_CHECK_STRING(cmd->arguments[4]->arg_name, "five");
    MOS_TEST_CHECK(cmd->arguments[4]->params_count, 0);
    MOS_TEST_CHECK_STRING(cmd->arguments[5]->arg_name, "six");
    MOS_TEST_CHECK(cmd->arguments[5]->params_count, 0);
    MOS_TEST_CHECK_STRING(cmd->arguments[6]->arg_name, "seven");
    MOS_TEST_CHECK(cmd->arguments[6]->params_count, 0);
    MOS_TEST_CHECK_STRING(cmd->arguments[7]->arg_name, "eight");
    MOS_TEST_CHECK(cmd->arguments[7]->params_count, 0);
    MOS_TEST_CHECK_STRING(cmd->arguments[8]->arg_name, "nine");
    MOS_TEST_CHECK(cmd->arguments[8]->params_count, 0);
    MOS_TEST_CHECK_STRING(cmd->arguments[9]->arg_name, "ten");
    MOS_TEST_CHECK(cmd->arguments[9]->params_count, 0);
    mos_cmdline_destroy(cmd);
}

MOS_TEST_CASE(one_arg_with_an_option)
{
    const char *one_arg_with_an_option = "one=nanana";
    cmdline_t *cmd = mos_cmdline_create(one_arg_with_an_option);
    MOS_TEST_CHECK(cmd != NULL, true);
    MOS_TEST_CHECK(cmd->args_count, 1);
    MOS_TEST_CHECK_STRING(cmd->arguments[0]->arg_name, "one");
    MOS_TEST_CHECK(cmd->arguments[0]->params_count, 1);
    MOS_TEST_CHECK(cmd->arguments[0]->params[0]->param_type, CMDLINE_PARAM_TYPE_STRING);
    MOS_TEST_CHECK_STRING(cmd->arguments[0]->params[0]->val.string, "nanana");
    mos_cmdline_destroy(cmd);
}

MOS_TEST_CASE(one_arg_with_multiple_options)
{
    const char *one_arg_with_multiple_options = "one=nana1,nana2,nana3,nana4,false";
    cmdline_t *cmd = mos_cmdline_create(one_arg_with_multiple_options);
    MOS_TEST_CHECK(cmd != NULL, true);
    MOS_TEST_CHECK(cmd->args_count, 1);
    MOS_TEST_CHECK_STRING(cmd->arguments[0]->arg_name, "one");
    MOS_TEST_CHECK(cmd->arguments[0]->params_count, 5);
    MOS_TEST_CHECK(cmd->arguments[0]->params[0]->param_type, CMDLINE_PARAM_TYPE_STRING);
    MOS_TEST_CHECK_STRING(cmd->arguments[0]->params[0]->val.string, "nana1");
    MOS_TEST_CHECK(cmd->arguments[0]->params[1]->param_type, CMDLINE_PARAM_TYPE_STRING);
    MOS_TEST_CHECK_STRING(cmd->arguments[0]->params[1]->val.string, "nana2");
    MOS_TEST_CHECK(cmd->arguments[0]->params[2]->param_type, CMDLINE_PARAM_TYPE_STRING);
    MOS_TEST_CHECK_STRING(cmd->arguments[0]->params[2]->val.string, "nana3");
    MOS_TEST_CHECK(cmd->arguments[0]->params[3]->param_type, CMDLINE_PARAM_TYPE_STRING);
    MOS_TEST_CHECK_STRING(cmd->arguments[0]->params[3]->val.string, "nana4");
    MOS_TEST_CHECK(cmd->arguments[0]->params[4]->param_type, CMDLINE_PARAM_TYPE_BOOL);
    MOS_TEST_CHECK(cmd->arguments[0]->params[4]->val.boolean, false);
    mos_cmdline_destroy(cmd);
}

MOS_TEST_CASE(multi_args_with_multiple_options)
{
    const char *cmdline = "one=nana1,nana2,nana3,nana4,false two=nana1,nana2,nana3,nana4,false three=nana1,nana2,nana3,nana4,true";
    cmdline_t *cmd = mos_cmdline_create(cmdline);
    MOS_TEST_CHECK(cmd != NULL, true);

    MOS_TEST_CHECK(cmd->args_count, 3);
    MOS_TEST_CHECK_STRING(cmd->arguments[0]->arg_name, "one");
    MOS_TEST_CHECK(cmd->arguments[0]->params_count, 5);
    MOS_TEST_CHECK(cmd->arguments[0]->params[0]->param_type, CMDLINE_PARAM_TYPE_STRING);
    MOS_TEST_CHECK_STRING(cmd->arguments[0]->params[0]->val.string, "nana1");
    MOS_TEST_CHECK(cmd->arguments[0]->params[1]->param_type, CMDLINE_PARAM_TYPE_STRING);
    MOS_TEST_CHECK_STRING(cmd->arguments[0]->params[1]->val.string, "nana2");
    MOS_TEST_CHECK(cmd->arguments[0]->params[2]->param_type, CMDLINE_PARAM_TYPE_STRING);
    MOS_TEST_CHECK_STRING(cmd->arguments[0]->params[2]->val.string, "nana3");
    MOS_TEST_CHECK(cmd->arguments[0]->params[3]->param_type, CMDLINE_PARAM_TYPE_STRING);
    MOS_TEST_CHECK_STRING(cmd->arguments[0]->params[3]->val.string, "nana4");
    MOS_TEST_CHECK(cmd->arguments[0]->params[4]->param_type, CMDLINE_PARAM_TYPE_BOOL);
    MOS_TEST_CHECK(cmd->arguments[0]->params[4]->val.boolean, false);

    MOS_TEST_CHECK_STRING(cmd->arguments[1]->arg_name, "two");
    MOS_TEST_CHECK(cmd->arguments[1]->params_count, 5);
    MOS_TEST_CHECK(cmd->arguments[1]->params[0]->param_type, CMDLINE_PARAM_TYPE_STRING);
    MOS_TEST_CHECK_STRING(cmd->arguments[1]->params[0]->val.string, "nana1");
    MOS_TEST_CHECK(cmd->arguments[1]->params[1]->param_type, CMDLINE_PARAM_TYPE_STRING);
    MOS_TEST_CHECK_STRING(cmd->arguments[1]->params[1]->val.string, "nana2");
    MOS_TEST_CHECK(cmd->arguments[1]->params[2]->param_type, CMDLINE_PARAM_TYPE_STRING);
    MOS_TEST_CHECK_STRING(cmd->arguments[1]->params[2]->val.string, "nana3");
    MOS_TEST_CHECK(cmd->arguments[1]->params[3]->param_type, CMDLINE_PARAM_TYPE_STRING);
    MOS_TEST_CHECK_STRING(cmd->arguments[1]->params[3]->val.string, "nana4");
    MOS_TEST_CHECK(cmd->arguments[1]->params[4]->param_type, CMDLINE_PARAM_TYPE_BOOL);
    MOS_TEST_CHECK(cmd->arguments[1]->params[4]->val.boolean, false);

    MOS_TEST_CHECK_STRING(cmd->arguments[2]->arg_name, "three");
    MOS_TEST_CHECK(cmd->arguments[2]->params_count, 5);
    MOS_TEST_CHECK(cmd->arguments[2]->params[0]->param_type, CMDLINE_PARAM_TYPE_STRING);
    MOS_TEST_CHECK_STRING(cmd->arguments[2]->params[0]->val.string, "nana1");
    MOS_TEST_CHECK(cmd->arguments[2]->params[1]->param_type, CMDLINE_PARAM_TYPE_STRING);
    MOS_TEST_CHECK_STRING(cmd->arguments[2]->params[1]->val.string, "nana2");
    MOS_TEST_CHECK(cmd->arguments[2]->params[2]->param_type, CMDLINE_PARAM_TYPE_STRING);
    MOS_TEST_CHECK_STRING(cmd->arguments[2]->params[2]->val.string, "nana3");
    MOS_TEST_CHECK(cmd->arguments[2]->params[3]->param_type, CMDLINE_PARAM_TYPE_STRING);
    MOS_TEST_CHECK_STRING(cmd->arguments[2]->params[3]->val.string, "nana4");
    MOS_TEST_CHECK(cmd->arguments[2]->params[4]->param_type, CMDLINE_PARAM_TYPE_BOOL);
    MOS_TEST_CHECK(cmd->arguments[2]->params[4]->val.boolean, true);
    mos_cmdline_destroy(cmd);
}
