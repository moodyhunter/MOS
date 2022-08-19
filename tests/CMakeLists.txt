# SPDX-License-Identifier: GPL-3.0-or-later
add_kernel_source(RELATIVE_SOURCES test_engine.c)

macro(mos_add_test TEST_NAME TEST_DESCRIPTION)
    set(TEST_TARGET mos_test_${TEST_NAME})
    add_library(${TEST_TARGET} OBJECT ${CMAKE_CURRENT_LIST_DIR}/${TEST_NAME}/test_${TEST_NAME}.c)

    if (${TEST_NAME} IN_LIST MOS_DISABLED_TESTS)
        set(MOS_TEST_${TEST_NAME}_STATE "disabled")
    else()
        set(MOS_TEST_${TEST_NAME}_STATE "enabled")
        target_link_libraries(mos_kernel.elf PRIVATE $<TARGET_OBJECTS:${TEST_TARGET}>)
    endif()

    mos_add_summary_item(TESTS "${TEST_DESCRIPTION}" "${MOS_TEST_${TEST_NAME}_STATE}")
endmacro()

mos_add_test(printf "printf family")
mos_add_test(linked_list "linked list")
mos_add_test(kmalloc "kmalloc stress test")
mos_add_test(cmdline_parser "cmdline parser test")
mos_add_test(hashmap "hashmap test")
