# SPDX-License-Identifier: GPL-3.0-or-later

# ! HACK: This is a hack to get the host C/C++ compiler.
set(CMAKE_C_COMPILER "/usr/bin/gcc")
set(CMAKE_CXX_COMPILER "/usr/bin/g++")
set(CMAKE_C_FLAGS "")
set(CMAKE_CXX_FLAGS "")
set(CMAKE_EXE_LINKER_FLAGS "")

cmake_minimum_required(VERSION 3.16)
project(MOS-Tests C CXX)

macro(mos_add_test TEST_NAME TEST_DESCRIPTION)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/${TEST_NAME})
    target_compile_options(${TEST_NAME} PRIVATE -Wall -Wextra -Wpedantic -pedantic)

    if (${TEST_NAME} IN_LIST MOS_DISABLED_TESTS)
        set(MOS_TEST_${TEST_NAME}_STATE "disabled")
    else()
        set(MOS_TEST_${TEST_NAME}_STATE "enabled")
    endif()

    mos_add_summary_item(TESTS "${TEST_DESCRIPTION}" "${MOS_TEST_${TEST_NAME}_STATE}")
    add_test(NAME MOS_TEST_${TEST_NAME} COMMAND $<TARGET_FILE:${TEST_NAME}>)
endmacro()

mos_add_test(printf "printf family")

