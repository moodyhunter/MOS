# SPDX-License-Identifier: GPL-3.0-or-later

include(cmake/toolchains/cross-i686.cmake)

cmake_minimum_required(VERSION 3.16)

project(MOS LANGUAGES C ASM VERSION 0.1)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_C_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if(EXISTS ${CMAKE_SOURCE_DIR}/config.cmake)
    message(STATUS "Found custom configuration file 'config.cmake'")
    include(${CMAKE_SOURCE_DIR}/config.cmake)
endif()

include(generate_kconfig)
include(configure_summary)

add_library(mos_kernel.elf OBJECT
    init/kernel_init.c
    drivers/screen.c
    drivers/port.c
    lib/stdlib.c
    lib/string.c
    lib/stdio.c
)

add_library(mos::elf_kernel ALIAS mos_kernel.elf)

generate_kconfig(mos_kernel.elf)

mos_add_summary_section(KERNEL "MOS Kernel")
mos_add_summary_item(KERNEL "Version" "${MOS_KERNEL_VERSION_STRING}")
mos_add_summary_item(KERNEL "Revision" "${MOS_KERNEL_REVISION_STRING}")
mos_add_summary_item(KERNEL "Builtin Cmdline" "${MOS_KERNEL_BUILTIN_CMDLINE_STRING}")

target_include_directories(mos_kernel.elf PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/include)

# Configure boot loader
include(boot/boot.cmake)

message("")
message("MOS is now configured :)")
message("")
mos_print_summary()
