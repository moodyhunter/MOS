// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_engine.h"

#include <mos/lib/cmdline.h>

MOS_TEST_CASE(simple_cmdline)
{
    char cmdline[256] = "one two three four five six seven eight nine ten";
    const char *cmdlines[11] = { 0 };
    size_t cmdlines_count = 0;
    cmdline_parse_inplace(cmdline, strlen(cmdline), 10, &cmdlines_count, cmdlines);
    MOS_TEST_CHECK(cmdlines_count, 10);
    MOS_TEST_CHECK_STRING(cmdlines[0], "one");
    MOS_TEST_CHECK_STRING(cmdlines[1], "two");
    MOS_TEST_CHECK_STRING(cmdlines[2], "three");
    MOS_TEST_CHECK_STRING(cmdlines[3], "four");
    MOS_TEST_CHECK_STRING(cmdlines[4], "five");
    MOS_TEST_CHECK_STRING(cmdlines[5], "six");
    MOS_TEST_CHECK_STRING(cmdlines[6], "seven");
    MOS_TEST_CHECK_STRING(cmdlines[7], "eight");
    MOS_TEST_CHECK_STRING(cmdlines[8], "nine");
    MOS_TEST_CHECK_STRING(cmdlines[9], "ten");
    MOS_TEST_CHECK(cmdlines[10], NULL);
}

MOS_TEST_CASE(one_arg_with_an_option)
{
    char cmdline[256] = "one=nana";
    const char *cmdlines[2] = { 0 };
    size_t cmdlines_count = 0;
    cmdline_parse_inplace(cmdline, strlen(cmdline), 1, &cmdlines_count, cmdlines);
    MOS_TEST_CHECK(cmdlines_count, 1);
    MOS_TEST_CHECK_STRING(cmdlines[0], "one=nana");
    MOS_TEST_CHECK(cmdlines[1], NULL);
}

MOS_TEST_CASE(one_arg_with_multiple_options)
{
    char cmdline[256] = "one=nana1,nana2,nana3,nana4,false";
    const char *cmdlines[2] = { 0 };
    size_t cmdlines_count = 0;
    cmdline_parse_inplace(cmdline, strlen(cmdline), 1, &cmdlines_count, cmdlines);
    MOS_TEST_CHECK(cmdlines_count, 1);
    MOS_TEST_CHECK_STRING(cmdlines[0], "one=nana1,nana2,nana3,nana4,false");
    MOS_TEST_CHECK(cmdlines[1], NULL);
}

MOS_TEST_CASE(multi_args_with_multiple_options)
{
    char cmdline[256] = "one=nana1,nana2,nana3,nana4,false two=nana1,nana2,nana3,nana4,false three=nana1,nana2,nana3,nana4,true";
    const char *cmdlines[4] = { 0 };
    size_t cmdlines_count = 0;
    cmdline_parse_inplace(cmdline, strlen(cmdline), 3, &cmdlines_count, cmdlines);
    MOS_TEST_CHECK(cmdlines_count, 3);
    MOS_TEST_CHECK_STRING(cmdlines[0], "one=nana1,nana2,nana3,nana4,false");
    MOS_TEST_CHECK_STRING(cmdlines[1], "two=nana1,nana2,nana3,nana4,false");
    MOS_TEST_CHECK_STRING(cmdlines[2], "three=nana1,nana2,nana3,nana4,true");
    MOS_TEST_CHECK(cmdlines[3], NULL);
}

MOS_TEST_CASE(quoted_args)
{
    char cmdline[256] = "one=\"nana1,nana2,nana3,nana4,false\" two=\"nana1,nana2,nana3,nana4,false\" three=\"nana1,nana2,nana3,nana4,true\"";
    const char *cmdlines[4] = { 0 };
    size_t cmdlines_count = 0;
    cmdline_parse_inplace(cmdline, strlen(cmdline), 3, &cmdlines_count, cmdlines);
    MOS_TEST_CHECK(cmdlines_count, 3);
    MOS_TEST_CHECK_STRING(cmdlines[0], "one=\"nana1,nana2,nana3,nana4,false\"");
    MOS_TEST_CHECK_STRING(cmdlines[1], "two=\"nana1,nana2,nana3,nana4,false\"");
    MOS_TEST_CHECK_STRING(cmdlines[2], "three=\"nana1,nana2,nana3,nana4,true\"");
    MOS_TEST_CHECK(cmdlines[3], NULL);
}

MOS_TEST_CASE(quoted_args_with_spaces)
{
    char cmdline[256] = "one=\"nana1 nana2 nana3 nana4 false\" two=\"nana1 nana2 nana3 nana4 false\" three=\"nana1 nana2 nana3 nana4 true\"";
    const char *cmdlines[4] = { 0 };
    size_t cmdlines_count = 0;
    cmdline_parse_inplace(cmdline, strlen(cmdline), 3, &cmdlines_count, cmdlines);
    MOS_TEST_CHECK(cmdlines_count, 3);
    MOS_TEST_CHECK_STRING(cmdlines[0], "one=\"nana1 nana2 nana3 nana4 false\"");
    MOS_TEST_CHECK_STRING(cmdlines[1], "two=\"nana1 nana2 nana3 nana4 false\"");
    MOS_TEST_CHECK_STRING(cmdlines[2], "three=\"nana1 nana2 nana3 nana4 true\"");
    MOS_TEST_CHECK(cmdlines[3], NULL);
}

MOS_TEST_CASE(quoted_args_with_spaces_and_commas)
{
    char cmdline[256] = "one=\"nana1 nana2,nana3 nana4 false\" two=\"nana1 nana2,nana3 nana4 false\" three=\"nana1 nana2,nana3 nana4 true\"";
    const char *cmdlines[4] = { 0 };
    size_t cmdlines_count = 0;
    cmdline_parse_inplace(cmdline, strlen(cmdline), 3, &cmdlines_count, cmdlines);
    MOS_TEST_CHECK(cmdlines_count, 3);
    MOS_TEST_CHECK_STRING(cmdlines[0], "one=\"nana1 nana2,nana3 nana4 false\"");
    MOS_TEST_CHECK_STRING(cmdlines[1], "two=\"nana1 nana2,nana3 nana4 false\"");
    MOS_TEST_CHECK_STRING(cmdlines[2], "three=\"nana1 nana2,nana3 nana4 true\"");
    MOS_TEST_CHECK(cmdlines[3], NULL);
}

MOS_TEST_CASE(quoted_args_with_spaces_and_commas_and_equals)
{
    char cmdline[256] = "one=\"nana1=nana2,nana3=nana4 false\" two=\"nana1=nana2,nana3=nana4 false\" three=\"nana1=nana2,nana3=nana4 true\"";
    const char *cmdlines[4] = { 0 };
    size_t cmdlines_count = 0;
    cmdline_parse_inplace(cmdline, strlen(cmdline), 3, &cmdlines_count, cmdlines);
    MOS_TEST_CHECK(cmdlines_count, 3);
    MOS_TEST_CHECK_STRING(cmdlines[0], "one=\"nana1=nana2,nana3=nana4 false\"");
    MOS_TEST_CHECK_STRING(cmdlines[1], "two=\"nana1=nana2,nana3=nana4 false\"");
    MOS_TEST_CHECK_STRING(cmdlines[2], "three=\"nana1=nana2,nana3=nana4 true\"");
    MOS_TEST_CHECK(cmdlines[3], NULL);
}

MOS_TEST_CASE(quotation_with_escaped_quotation_marks)
{
    char cmdline[256] =
        "one=\"nana1=\\\"nana2\\\",nana3=\\\"nana4\\\" false\" two=\"nana1=\\\"nana2\\\",nana3=\\\"nana4\\\" false\" three=\"nana1=\\\"nana2\\\",nana3=\\\"nana4\\\" true\"";
    const char *cmdlines[4] = { 0 };
    size_t cmdlines_count = 0;
    cmdline_parse_inplace(cmdline, strlen(cmdline), 3, &cmdlines_count, cmdlines);
    MOS_TEST_CHECK(cmdlines_count, 3);
    MOS_TEST_CHECK_STRING(cmdlines[0], "one=\"nana1=\\\"nana2\\\",nana3=\\\"nana4\\\" false\"");
    MOS_TEST_CHECK_STRING(cmdlines[1], "two=\"nana1=\\\"nana2\\\",nana3=\\\"nana4\\\" false\"");
    MOS_TEST_CHECK_STRING(cmdlines[2], "three=\"nana1=\\\"nana2\\\",nana3=\\\"nana4\\\" true\"");
    MOS_TEST_CHECK(cmdlines[3], NULL);
}
