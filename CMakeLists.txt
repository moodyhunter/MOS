# SPDX-License-Identifier: GPL-3.0-or-later

include(cmake/toolchains/cross-i686.cmake)

cmake_minimum_required(VERSION 3.22)

project(MOS LANGUAGES CXX C ASM VERSION 0.1)
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

if(EXISTS ${CMAKE_SOURCE_DIR}/config.cmake)
    message(STATUS "Found custom configuration file 'config.cmake'")
    include(${CMAKE_SOURCE_DIR}/config.cmake)
endif()

include(CTest)
include(generate_kconfig)
include(configure_summary)

add_library(mos_kernel.elf STATIC
    init/kernel_init.c
    drivers/screen.c
    drivers/port.c
    kernel/panic.c
    kernel/printk.c
    lib/stdlib.c
    lib/string.c
    lib/stdio.c
    lib/stdio_impl.c
)

add_library(mos::elf_kernel ALIAS mos_kernel.elf)
target_link_libraries(mos_kernel.elf PUBLIC gcc)

generate_kconfig(mos_kernel.elf)

mos_add_summary_section(KERNEL "MOS Kernel")
mos_add_summary_item(KERNEL "Version" "${MOS_KERNEL_VERSION_STRING}")
mos_add_summary_item(KERNEL "Revision" "${MOS_KERNEL_REVISION_STRING}")
mos_add_summary_item(KERNEL "Builtin Cmdline" "${MOS_KERNEL_BUILTIN_CMDLINE_STRING}")

target_include_directories(mos_kernel.elf PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/include)

# Configure boot loader
include(boot/boot.cmake)

mos_add_summary_section(TESTS "MOS Kernel Library Tests")
mos_add_summary_item(TESTS "All Tests" "${BUILD_TESTING}")

if(BUILD_TESTING)
add_subdirectory(tests)
# include(tests/tests.cmake)
endif()

message("")
message("MOS is now configured :)")
message("")
mos_print_summary()
