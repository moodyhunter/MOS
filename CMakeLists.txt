# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.16)
project(MOS LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_C_EXTENSIONS OFF)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

list(APPEND CMAKE_C_FLAGS "-fno-pie -m32 -ffreestanding")
set(MOS_KERNEL_BINARY ${CMAKE_BINARY_DIR}/mos_kernel.bin)

# include config.cmake if it exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/config.cmake)
    message(STATUS "Found custom configuration file 'config.cmake'")
    include(${CMAKE_CURRENT_SOURCE_DIR}/config.cmake)
endif()

if(NOT MOS_BOOT_METHOD)
    set(MOS_BOOT_METHOD "raw")
endif()

include(add_nasm_binary)
include(add_kernel_binary)

add_library(mos_kernel_object OBJECT
    src/kernel.c
    src/stdio.c)

target_link_libraries(mos_kernel_object PRIVATE gcc)
add_library(mos::kernel_object ALIAS mos_kernel_object)

# Configure boot loader
include(arch/x86/boot.cmake)

# add_custom_target(mos_kernel ALL DEPENDS ${MOS_KERNEL_BINARY})

if(NOT TARGET mos_kernel)
    message(FATAL_ERROR "No target mos_kernel defined, possibly invalid boot configuration")
endif()

message(STATUS "MOS is now configured.")
message("")
message("Boot Type: ${MOS_BOOT_METHOD}")
message("")
