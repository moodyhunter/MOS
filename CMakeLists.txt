# SPDX-License-Identifier: GPL-3.0-or-later

include(cmake/toolchains/cross-i686.cmake)

cmake_minimum_required(VERSION 3.22)

project(MOS LANGUAGES CXX C ASM_NASM VERSION 0.1)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -pedantic")

# set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
# set(CMAKE_CXX_EXTENSIONS OFF)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -pedantic")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(MOS_KERNEL_RUN_TESTS "Run tests in the Kernel" OFF)
set(MOS_ARCH_TARGET "x86")

if(EXISTS ${CMAKE_SOURCE_DIR}/config.cmake)
    message(STATUS "Found custom configuration file 'config.cmake'")
    include(${CMAKE_SOURCE_DIR}/config.cmake)
endif()

set(MOS_ARCH_CONFIG ${CMAKE_SOURCE_DIR}/arch/${MOS_ARCH_TARGET}/${MOS_ARCH_TARGET}.cmake)

if (NOT EXISTS ${MOS_ARCH_CONFIG})
    message(FATAL_ERROR "No configuration file for architecture '${MOS_ARCH_TARGET}'")
endif()

include(CTest)
include(generate_kconfig)
include(configure_summary)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_SOURCE_DIR}/arch/${MOS_ARCH_TARGET}/include
)

add_library(mos_kernel.elf STATIC
    kernel/device/console.c
    kernel/kernel_init.c
    kernel/panic.c
    kernel/printk.c
    kernel/interrupt.c
    lib/stdlib.c
    lib/string.c
    lib/stdio.c
    lib/stdio_impl.c
    lib/linked_list.c
)

add_library(mos::elf_kernel ALIAS mos_kernel.elf)
target_link_libraries(mos_kernel.elf PUBLIC gcc)

generate_kconfig(mos_kernel.elf)

mos_add_summary_section(KERNEL "MOS Kernel")
mos_add_summary_item(KERNEL "Version" "${MOS_KERNEL_VERSION_STRING}")
mos_add_summary_item(KERNEL "Revision" "${MOS_KERNEL_REVISION_STRING}")
mos_add_summary_item(KERNEL "Builtin Cmdline" "${MOS_KERNEL_BUILTIN_CMDLINE_STRING}")
mos_add_summary_item(KERNEL "Architecture" "${MOS_ARCH_TARGET}")

# Configure architecture-specific stuff
include(add_kernel_source)
include(add_bootable_target)

include(${MOS_ARCH_CONFIG})

mos_add_summary_section(TESTS "MOS Kernel Library Tests")
mos_add_summary_item(TESTS "All Tests" "${BUILD_TESTING}")
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

message("")
message("MOS is now configured :)")
message("")

mos_print_summary()
dump_bootable_targets()
