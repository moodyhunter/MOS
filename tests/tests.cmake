# SPDX-License-Identifier: GPL-3.0-or-later

macro(mos_add_test TEST_NAME TEST_DESCRIPTION)
    add_library(mos_test_${TEST_NAME} STATIC ${CMAKE_CURRENT_LIST_DIR}/${TEST_NAME}/test.c)
    target_link_libraries(mos_kernel.elf INTERFACE mos_test_${TEST_NAME})
    target_include_directories(mos_test_${TEST_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/include)

    if (${TEST_NAME} IN_LIST MOS_DISABLED_TESTS)
        set(MOS_TEST_${TEST_NAME}_STATE "disabled")
    else()
        set(MOS_TEST_${TEST_NAME}_STATE "enabled")
    endif()

    mos_add_summary_item(TESTS "${TEST_DESCRIPTION}" "${MOS_TEST_${TEST_NAME}_STATE}")
endmacro()

mos_add_test(printf "printf family")

