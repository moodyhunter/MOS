# SPDX-License-Identifier: GPL-3.0-or-later

add_library(mos_test_engine STATIC ${CMAKE_CURRENT_LIST_DIR}/test_engine.c)
add_library(mos::test_engine ALIAS mos_test_engine)
target_include_directories(mos_test_engine PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(mos_kernel.elf INTERFACE mos_test_engine)

macro(mos_add_test TEST_NAME TEST_DESCRIPTION)
    set(TEST_TARGET mos_test_${TEST_NAME})
    add_library(${TEST_TARGET} STATIC ${CMAKE_CURRENT_LIST_DIR}/${TEST_NAME}/test.c)
    target_include_directories(${TEST_TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_include_directories(${TEST_TARGET} PRIVATE ${CMAKE_CURRENT_LIST_DIR})

    # CMake 3.24, WHOLE_ARCHIVE is used to include the whole archive into the kernel.
    # target_link_libraries(mos_test_engine PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,${TEST_TARGET}>)
    target_link_options(mos_test_engine PUBLIC "-Wl,--whole-archive" $<TARGET_FILE:${TEST_TARGET}> "-Wl,--no-whole-archive")
    # Currently a hack to add dependencies.
    target_link_libraries(mos_test_engine PUBLIC ${TEST_TARGET})

    if (${TEST_NAME} IN_LIST MOS_DISABLED_TESTS)
        set(MOS_TEST_${TEST_NAME}_STATE "disabled")
    else()
        set(MOS_TEST_${TEST_NAME}_STATE "enabled")
        list(APPEND MOS_ALL_TESTS ${TEST_NAME})
    endif()

    # file(WRITE )

    mos_add_summary_item(TESTS "${TEST_DESCRIPTION}" "${MOS_TEST_${TEST_NAME}_STATE}")
endmacro()

mos_add_test(printf "printf family")

