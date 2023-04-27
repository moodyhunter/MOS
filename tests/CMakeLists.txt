# SPDX-License-Identifier: GPL-3.0-or-later

mos_kconfig(TESTS   BUILD_TESTING   ON "Kernel Unit Tests")

if (BUILD_TESTING)
    message(STATUS "MOS kernel unit tests are enabled")
    add_kernel_source(RELATIVE_SOURCES test_engine.c)
    target_include_directories(mos_kernel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()

macro(mos_add_test TEST_NAME TEST_DESCRIPTION)
    set(TEST_TARGET _test_${TEST_NAME})
    mos_kconfig(TESTS TEST_${TEST_NAME} ${BUILD_TESTING} "Enable ${TEST_DESCRIPTION} test")

    if (TEST_${TEST_NAME})
        add_kernel_source(SOURCES ${CMAKE_CURRENT_LIST_DIR}/${TEST_NAME}/test_${TEST_NAME}.c)
    endif()
endmacro()

mos_add_test(printf "printf family")
mos_add_test(linked_list "linked list")
mos_add_test(kmalloc "kmalloc")
mos_add_test(cmdline_parser "cmdline parser")
mos_add_test(hashmap "hashmap")
mos_add_test(downwards_stack "downwards stack")
mos_add_test(memops "memery operations")
mos_add_test(ring_buffer "ring buffer")
