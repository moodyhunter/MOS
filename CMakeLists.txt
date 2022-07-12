# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.16)
project(MOS LANGUAGES C ASM VERSION 0.1)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_LINK_EXECUTABLE "ld <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

list(APPEND CMAKE_C_FLAGS "-fno-pie -m32 -ffreestanding -nostdlib -nostdinc")
set(MOS_KERNEL_BINARY ${CMAKE_BINARY_DIR}/mos_kernel.bin)

if(EXISTS ${CMAKE_SOURCE_DIR}/config.cmake)
    message(STATUS "Found custom configuration file 'config.cmake'")
    # include config.cmake if it exists
    include(${CMAKE_SOURCE_DIR}/config.cmake)
endif()

if(NOT MOS_BOOT_METHOD)
    message(WARNING "No boot method specified, using default 'raw'")
    set(MOS_BOOT_METHOD "raw")
endif()

add_library(mos_kernel_object OBJECT
    init/kernel_init.c
    drivers/screen.c
    lib/stdlib.c
)

target_include_directories(mos_kernel_object PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(mos_kernel_object PUBLIC ${CMAKE_BINARY_DIR}/include)
target_link_libraries(mos_kernel_object PRIVATE gcc)
add_library(mos::kernel_object ALIAS mos_kernel_object)

include(add_nasm_binary)
include(add_kernel_binary)
include(generate_kconfig)

generate_kconfig()

# Configure boot loader
include(arch/x86/boot.cmake)

if(NOT TARGET mos_kernel.bin)
    message(FATAL_ERROR "No target mos_kernel defined, possibly invalid boot configuration, did you call 'add_kernel_binary'?")
endif()

message(STATUS "MOS is now configured.")
