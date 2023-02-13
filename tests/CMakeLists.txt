# SPDX-License-Identifier: GPL-3.0-or-later

mos_kconfig(TESTS   BUILD_TESTING   OFF "Kernel Unit Tests")

if (BUILD_TESTING)
    message(STATUS "MOS kernel unit tests are enabled")
endif()

add_kernel_source(RELATIVE_SOURCES test_engine.c)
target_include_directories(mos_kernel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

macro(mos_add_test TEST_NAME TEST_DESCRIPTION)
    set(TEST_TARGET mos_test_${TEST_NAME})

    add_library(${TEST_TARGET} OBJECT ${CMAKE_CURRENT_LIST_DIR}/${TEST_NAME}/test_${TEST_NAME}.c)
    target_compile_definitions(${TEST_TARGET} PRIVATE -D__MOS_KERNEL__)
    target_include_directories(${TEST_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${TEST_TARGET} PRIVATE mos::include mos::private_include)

    mos_kconfig(TESTS   TEST_${TEST_NAME} ${BUILD_TESTING} "Enable ${TEST_DESCRIPTION} test")

    if (TEST_${TEST_NAME})
        target_link_libraries(mos_kernel PRIVATE $<TARGET_OBJECTS:${TEST_TARGET}>)
    endif()
endmacro()

mos_add_test(printf "printf family")
mos_add_test(linked_list "linked list")
mos_add_test(kmalloc "kmalloc stress test")
mos_add_test(cmdline_parser "cmdline parser test")
mos_add_test(hashmap "hashmap test")
mos_add_test(downwards_stack "downwards stack test")
mos_add_test(memops "test memery operations")
mos_add_test(ring_buffer "test ring buffer")
