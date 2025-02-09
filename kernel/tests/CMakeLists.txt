# SPDX-License-Identifier: GPL-3.0-or-later

if (MOS_TESTS)
    message(STATUS "Kernel space unit tests are enabled")
    add_kernel_source(test_engine.cpp)
    target_include_directories(mos_kernel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
else()
    return()
endif()

macro(mos_add_test TEST_NAME)
    set(TEST_TARGET _test_${TEST_NAME})

    if (MOS_TEST_${TEST_NAME})
        add_kernel_source(${CMAKE_CURRENT_LIST_DIR}/${TEST_NAME}/test_${TEST_NAME}.cpp)
    endif()
endmacro()

mos_add_test(printf)
mos_add_test(linked_list)
mos_add_test(kmalloc)
mos_add_test(cmdline_parser)
mos_add_test(hashmap)
mos_add_test(downwards_stack)
mos_add_test(memops)
mos_add_test(ring_buffer)
mos_add_test(vfs)
