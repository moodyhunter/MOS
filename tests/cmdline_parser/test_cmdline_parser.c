// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/cmdline.h"
#include "test_engine.h"

MOS_TEST_CASE(simple_cmdline)
{
    const char *single = "single";
    cmdline_t *cmd = cmdline_create(single);
    MOS_TEST_CHECK(cmd != NULL, true);
    MOS_TEST_CHECK(cmd->options_count, 1);
    MOS_TEST_CHECK_STRING(cmd->options[0]->name, "single");
    MOS_TEST_CHECK(cmd->options[0]->argc, 0);
    cmdline_destroy(cmd);
}

MOS_TEST_CASE(two_arguments)
{
    const char *two = "one two";
    cmdline_t *cmd = cmdline_create(two);
    MOS_TEST_CHECK(cmd != NULL, true);
    MOS_TEST_CHECK(cmd->options_count, 2);
    MOS_TEST_CHECK_STRING(cmd->options[0]->name, "one");
    MOS_TEST_CHECK(cmd->options[0]->argc, 0);
    MOS_TEST_CHECK_STRING(cmd->options[1]->name, "two");
    MOS_TEST_CHECK(cmd->options[1]->argc, 0);
    cmdline_destroy(cmd);
}

MOS_TEST_CASE(many_arguments)
{
    const char *many = "one two three four five six seven eight nine ten";
    cmdline_t *cmd = cmdline_create(many);
    MOS_TEST_CHECK(cmd != NULL, true);
    MOS_TEST_CHECK(cmd->options_count, 10);
    MOS_TEST_CHECK_STRING(cmd->options[0]->name, "one");
    MOS_TEST_CHECK(cmd->options[0]->argc, 0);
    MOS_TEST_CHECK_STRING(cmd->options[1]->name, "two");
    MOS_TEST_CHECK(cmd->options[1]->argc, 0);
    MOS_TEST_CHECK_STRING(cmd->options[2]->name, "three");
    MOS_TEST_CHECK(cmd->options[2]->argc, 0);
    MOS_TEST_CHECK_STRING(cmd->options[3]->name, "four");
    MOS_TEST_CHECK(cmd->options[3]->argc, 0);
    MOS_TEST_CHECK_STRING(cmd->options[4]->name, "five");
    MOS_TEST_CHECK(cmd->options[4]->argc, 0);
    MOS_TEST_CHECK_STRING(cmd->options[5]->name, "six");
    MOS_TEST_CHECK(cmd->options[5]->argc, 0);
    MOS_TEST_CHECK_STRING(cmd->options[6]->name, "seven");
    MOS_TEST_CHECK(cmd->options[6]->argc, 0);
    MOS_TEST_CHECK_STRING(cmd->options[7]->name, "eight");
    MOS_TEST_CHECK(cmd->options[7]->argc, 0);
    MOS_TEST_CHECK_STRING(cmd->options[8]->name, "nine");
    MOS_TEST_CHECK(cmd->options[8]->argc, 0);
    MOS_TEST_CHECK_STRING(cmd->options[9]->name, "ten");
    MOS_TEST_CHECK(cmd->options[9]->argc, 0);
    cmdline_destroy(cmd);
}

MOS_TEST_CASE(one_arg_with_an_option)
{
    const char *one_arg_with_an_option = "one=nanana";
    cmdline_t *cmd = cmdline_create(one_arg_with_an_option);
    MOS_TEST_CHECK(cmd != NULL, true);
    MOS_TEST_CHECK(cmd->options_count, 1);
    MOS_TEST_CHECK_STRING(cmd->options[0]->name, "one");
    MOS_TEST_CHECK(cmd->options[0]->argc, 1);
    MOS_TEST_CHECK_STRING(cmd->options[0]->argv[0], "nanana");
    cmdline_destroy(cmd);
}

MOS_TEST_CASE(one_arg_with_multiple_options)
{
    const char *one_arg_with_multiple_options = "one=nana1,nana2,nana3,nana4,false";
    cmdline_t *cmd = cmdline_create(one_arg_with_multiple_options);
    MOS_TEST_CHECK(cmd != NULL, true);
    MOS_TEST_CHECK(cmd->options_count, 1);
    MOS_TEST_CHECK_STRING(cmd->options[0]->name, "one");
    MOS_TEST_CHECK(cmd->options[0]->argc, 5);
    MOS_TEST_CHECK_STRING(cmd->options[0]->argv[0], "nana1");
    MOS_TEST_CHECK_STRING(cmd->options[0]->argv[1], "nana2");
    MOS_TEST_CHECK_STRING(cmd->options[0]->argv[2], "nana3");
    MOS_TEST_CHECK_STRING(cmd->options[0]->argv[3], "nana4");
    MOS_TEST_CHECK_STRING(cmd->options[0]->argv[4], "false");
    cmdline_destroy(cmd);
}

MOS_TEST_CASE(multi_args_with_multiple_options)
{
    const char *cmdline = "one=nana1,nana2,nana3,nana4,false two=nana1,nana2,nana3,nana4,false three=nana1,nana2,nana3,nana4,true";
    cmdline_t *cmd = cmdline_create(cmdline);
    MOS_TEST_CHECK(cmd != NULL, true);

    MOS_TEST_CHECK(cmd->options_count, 3);
    MOS_TEST_CHECK_STRING(cmd->options[0]->name, "one");
    MOS_TEST_CHECK(cmd->options[0]->argc, 5);
    MOS_TEST_CHECK_STRING(cmd->options[0]->argv[0], "nana1");
    MOS_TEST_CHECK_STRING(cmd->options[0]->argv[1], "nana2");
    MOS_TEST_CHECK_STRING(cmd->options[0]->argv[2], "nana3");
    MOS_TEST_CHECK_STRING(cmd->options[0]->argv[3], "nana4");
    MOS_TEST_CHECK_STRING(cmd->options[0]->argv[4], "false");

    MOS_TEST_CHECK_STRING(cmd->options[1]->name, "two");
    MOS_TEST_CHECK(cmd->options[1]->argc, 5);
    MOS_TEST_CHECK_STRING(cmd->options[1]->argv[0], "nana1");
    MOS_TEST_CHECK_STRING(cmd->options[1]->argv[1], "nana2");
    MOS_TEST_CHECK_STRING(cmd->options[1]->argv[2], "nana3");
    MOS_TEST_CHECK_STRING(cmd->options[1]->argv[3], "nana4");
    MOS_TEST_CHECK_STRING(cmd->options[1]->argv[4], "false");

    MOS_TEST_CHECK_STRING(cmd->options[2]->name, "three");
    MOS_TEST_CHECK(cmd->options[2]->argc, 5);
    MOS_TEST_CHECK_STRING(cmd->options[2]->argv[0], "nana1");
    MOS_TEST_CHECK_STRING(cmd->options[2]->argv[1], "nana2");
    MOS_TEST_CHECK_STRING(cmd->options[2]->argv[2], "nana3");
    MOS_TEST_CHECK_STRING(cmd->options[2]->argv[3], "nana4");
    MOS_TEST_CHECK_STRING(cmd->options[2]->argv[4], "true");
    cmdline_destroy(cmd);
}
