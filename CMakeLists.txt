# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.16)
project(MOS LANGUAGES C ASM VERSION 0.1)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_C_EXTENSIONS OFF)

# A side-effect of this is that `target_link_libraries` passes `-Wl,...` to the linker
# where ld cannot recognize.
set(CMAKE_C_LINK_EXECUTABLE "ld <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
list(APPEND CMAKE_C_FLAGS "-m32 -ffreestanding -nostdlib -nostdinc")
list(APPEND CMAKE_EXE_LINKER_FLAGS "-melf_i386")

if(EXISTS ${CMAKE_SOURCE_DIR}/config.cmake)
    message(STATUS "Found custom configuration file 'config.cmake'")
    include(${CMAKE_SOURCE_DIR}/config.cmake)
endif()

include(add_nasm_binary)
include(prepare_bootable_kernel_binary)
include(generate_kconfig)

add_library(mos_kernel.elf OBJECT
    init/kernel_init.c
    drivers/screen.c
    drivers/port.c
    lib/stdlib.c
)

add_library(mos::elf_kernel ALIAS mos_kernel.elf)

generate_kconfig(mos_kernel.elf)

# Used by the raw kernel image
target_compile_options(mos_kernel.elf PRIVATE -fno-pie -r)

set(MOS_KERNEL_LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/init/kernel.ld)
target_include_directories(mos_kernel.elf PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/include)
target_link_options(mos_kernel.elf PRIVATE -T${MOS_KERNEL_LINKER_SCRIPT})
set_target_properties(mos_kernel.elf PROPERTIES LINK_DEPENDS ${MOS_KERNEL_LINKER_SCRIPT})

# Configure boot loader
set(MOS_BOOTABLE_TARGETS)
set(MOS_BOOTABLE_TARGETS_FILE)
include(arch/x86/boot.cmake)

message("")
message("MOS is now configured :) ")
message("  Bootable targets: ")
foreach(target ${MOS_BOOTABLE_TARGETS})
    message("    - ${target}: ${MOS_BOOTABLE_TARGETS_FILE_${target}}")
endforeach()
message("")

