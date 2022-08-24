# SPDX-License-Identifier: GPL-3.0-or-later

include(cmake/toolchains/cross-i686.cmake)

cmake_minimum_required(VERSION 3.22)

project(MOS LANGUAGES CXX C ASM_NASM VERSION 0.1)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -pedantic -fstack-protector-strong")

# set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
# set(CMAKE_CXX_EXTENSIONS OFF)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -pedantic")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(MOS_ARCH_TARGET "x86")
option(MOS_MM_LIBALLOC_LOCKS "liballoc: has lock support" OFF)
option(MOS_MM_LIBALLOC_DEBUG "liballoc: debug message enabled" OFF)
option(MOS_ENABLE_DEBUG_LOG "Enable debug log" OFF)
option(MOS_PRINTK_HAS_FILENAME "printk: has filename" ON)

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

execute_process(
    COMMAND git describe --long --tags --all --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE MOS_KERNEL_REVISION_STRING
    ERROR_VARIABLE _DROP_
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)

mos_add_kconfig_define(MOS_MM_LIBALLOC_LOCKS)
mos_add_kconfig_define(MOS_MM_LIBALLOC_DEBUG)
mos_add_kconfig_define(MOS_ENABLE_DEBUG_LOG)
mos_add_kconfig_define(MOS_PRINTK_HAS_FILENAME)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

add_library(mos_kernel.elf STATIC
    kernel/cmdline.c
    kernel/device/console.c
    kernel/device/block.c
    kernel/elf/elf.c
    kernel/filesystem/cpio/cpio.c
    kernel/filesystem/filesystem.c
    kernel/filesystem/mount.c
    kernel/filesystem/pathutils.c
    kernel/interrupt.c
    kernel/init/main.c
    kernel/io/io.c
    kernel/kernel_init.c
    kernel/mm/liballoc.c
    kernel/mm/paging.c
    kernel/panic.c
    kernel/printk.c
    kernel/stack_protector.c
    kernel/tasks/process.c
    kernel/tasks/thread.c
    lib/structures/hashmap.c
    lib/structures/hashmap_common.c
    lib/structures/list.c
    lib/structures/stack.c
    lib/structures/tree.c
    lib/stdio.c
    lib/stdio_impl.c
    lib/stdlib.c
    lib/string.c
)

add_library(mos::elf_kernel ALIAS mos_kernel.elf)
target_link_libraries(mos_kernel.elf PUBLIC gcc)

mos_add_summary_section(KERNEL "MOS Kernel")
mos_add_summary_item(KERNEL "Version" "${MOS_KERNEL_VERSION_STRING}")
mos_add_summary_item(KERNEL "Revision" "${MOS_KERNEL_REVISION_STRING}")
mos_add_summary_item(KERNEL "Builtin Cmdline" "${MOS_KERNEL_BUILTIN_CMDLINE_STRING}")
mos_add_summary_item(KERNEL "Architecture" "${MOS_ARCH_TARGET}")

mos_add_summary_section(MM "MOS Memory Management")
mos_add_summary_item(MM "liballoc: locks" "${MOS_MM_LIBALLOC_LOCKS}")
mos_add_summary_item(MM "liballoc: debug" "${MOS_MM_LIBALLOC_DEBUG}")

mos_add_summary_section(DEBUG "MOS Debugging")
mos_add_summary_item(DEBUG "Debug log" "${MOS_ENABLE_DEBUG_LOG}")
mos_add_summary_item(DEBUG "printk: with filename" "${MOS_PRINTK_HAS_FILENAME}")

# Configure architecture-specific stuff
include(add_kernel_source)
include(add_bootable_target)

include(${MOS_ARCH_CONFIG})

mos_add_summary_section(TESTS "MOS Kernel Library Tests")
mos_add_summary_item(TESTS "All Tests" "${BUILD_TESTING}")
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

generate_kconfig(mos_kernel.elf)

message("")
message("MOS is now configured :)")
message("")

mos_print_summary()
dump_bootable_targets()
