# SPDX-License-Identifier: GPL-3.0-or-later

if (BUILD_TESTING)
    message(STATUS "MOS kernel unit tests are enabled")
    message(STATUS "Specific unit tests can be disabled with -DMOS_TESTING_DISABLE_<test_name>=ON")
endif()

add_kernel_source(RELATIVE_SOURCES test_engine.c)
target_include_directories(mos_kernel.elf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

macro(mos_add_test TEST_NAME TEST_DESCRIPTION)
    set(TEST_TARGET mos_test_${TEST_NAME})
    add_library(${TEST_TARGET} OBJECT ${CMAKE_CURRENT_LIST_DIR}/${TEST_NAME}/test_${TEST_NAME}.c)
    target_compile_definitions(${TEST_TARGET} PRIVATE -D__MOS_KERNEL__)
    target_include_directories(${TEST_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${TEST_TARGET} PRIVATE mos::basic_include mos::platform_include)

    if (MOS_TESTING_DISABLE_${TEST_NAME})
        set(MOS_TEST_${TEST_NAME}_STATE "disabled")
    elseif (NOT BUILD_TESTING)
        set(MOS_TEST_${TEST_NAME}_STATE "disabled globally")
    else()
        set(MOS_TEST_${TEST_NAME}_STATE "enabled")
        target_link_libraries(mos_kernel.elf PRIVATE $<TARGET_OBJECTS:${TEST_TARGET}>)
    endif()

    mos_add_summary_item(TESTS ${TEST_NAME} "${TEST_DESCRIPTION}" "${MOS_TEST_${TEST_NAME}_STATE}")
endmacro()

mos_add_test(printf "printf family")
mos_add_test(linked_list "linked list")
mos_add_test(kmalloc "kmalloc stress test")
mos_add_test(cmdline_parser "cmdline parser test")
mos_add_test(hashmap "hashmap test")
mos_add_test(downwards_stack "downwards stack test")
mos_add_test(fs_api "test fs api")
mos_add_test(memops "test memery operations")
mos_add_test(ring_buffer "test ring buffer")
