# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.16)
project(MOS LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_C_EXTENSIONS OFF)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

list(APPEND CMAKE_C_FLAGS "-fno-pie -m32 -ffreestanding")

# include config.cmake if it exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/config.cmake)
    message(STATUS "Found custom configuration file 'config.cmake'")
    include(${CMAKE_CURRENT_SOURCE_DIR}/config.cmake)
endif()

if(NOT MOS_BOOT_METHOD)
    set(MOS_BOOT_METHOD "raw")
endif()

include(add_nasm_binary)

add_library(mos_kernel_object OBJECT
    src/kernel.c
    src/stdio.c)

add_library(mos::kernel_object ALIAS mos_kernel_object)


# Configure boot loader
if(MOS_BOOT_METHOD STREQUAL "raw")
    include(arch/x86/boot.cmake)
endif()

add_custom_target(mos_kernel            DEPENDS ${CMAKE_BINARY_DIR}/mos_kernel.bin)
add_custom_target(mos_kernel_image ALL  DEPENDS ${CMAKE_BINARY_DIR}/mos_kernel.img)

message(STATUS "MOS is now configured.")
