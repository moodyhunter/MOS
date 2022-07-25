# SPDX-License-Identifier: GPL-3.0-or-later

# ! HACK: This is a hack to get the host C/C++ compiler.
set(CMAKE_ASM_COMPILER "/usr/bin/gcc")
set(CMAKE_LINKER "/usr/bin/ld")
set(CMAKE_C_COMPILER "/usr/bin/gcc")
set(CMAKE_CXX_COMPILER "/usr/bin/g++")
set(CMAKE_C_IMPLICIT_LINK_DIRECTORIES "/usr/lib/gcc/x86_64-pc-linux-gnu/12.1.0;/usr/lib")
string(REPLACE "-ffreestanding" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
string(REPLACE "-ffreestanding" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
string(REPLACE "-nostdlib" "" CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS})

cmake_minimum_required(VERSION 3.22)
project(MOS-Tests C CXX)

macro(mos_add_test TEST_NAME TEST_DESCRIPTION)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/${TEST_NAME})
    target_compile_options(${TEST_NAME} PRIVATE -Wall -Wextra -Wpedantic -pedantic)
    target_include_directories(${TEST_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
    target_link_libraries(${TEST_NAME} PUBLIC mos-libkernel)

    if (${TEST_NAME} IN_LIST MOS_DISABLED_TESTS)
        set(MOS_TEST_${TEST_NAME}_STATE "disabled")
    else()
        set(MOS_TEST_${TEST_NAME}_STATE "enabled")
    endif()

    mos_add_summary_item(TESTS "${TEST_DESCRIPTION}" "${MOS_TEST_${TEST_NAME}_STATE}")
    add_test(NAME MOS_TEST_${TEST_NAME} COMMAND $<TARGET_FILE:${TEST_NAME}>)
endmacro()

add_library(mos-libkernel OBJECT ../lib/stdio.c)
target_include_directories(mos-libkernel PUBLIC ${CMAKE_SOURCE_DIR}/include)

message(STATUS "My project sources: ${MY_PROJECT_SOURCES}")

mos_add_test(printf "printf family")

