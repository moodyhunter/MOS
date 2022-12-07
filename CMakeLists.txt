# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)

include(cmake/kconfig.cmake)
include(cmake/mos_target_setup.cmake)
include(mos_config.cmake)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Please create a separate build directory and run CMake from there.")
endif()

project(MOS LANGUAGES CXX C ASM_NASM VERSION 0.1)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -pedantic")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -pedantic")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(MOS_ARCH_CONFIG ${CMAKE_SOURCE_DIR}/arch/${MOS_ISA_FAMILY}/mos_platform.cmake)

if (NOT EXISTS ${MOS_ARCH_CONFIG})
    message(FATAL_ERROR "No configuration file for architecture '${MOS_ISA_FAMILY}'")
endif()

include(generate_syscall_headers)

add_library(mos_kernel_basic_include INTERFACE)
add_library(mos_kernel_platform_include INTERFACE)

target_include_directories(mos_kernel_basic_include INTERFACE ${CMAKE_SOURCE_DIR}/kernel/include ${CMAKE_BINARY_DIR}/include ${CMAKE_SOURCE_DIR}/lib/include)
target_include_directories(mos_kernel_platform_include INTERFACE ${CMAKE_SOURCE_DIR}/arch/${MOS_ISA_FAMILY}/include)

add_library(mos::basic_include ALIAS mos_kernel_basic_include)
add_library(mos::platform_include ALIAS mos_kernel_platform_include)

set(MOS_STANDARD_LIBRARY_SOURCES
    lib/structures/hashmap.c
    lib/structures/hashmap_common.c
    lib/structures/list.c
    lib/structures/stack.c
    lib/structures/ring_buffer.c
    lib/structures/tree.c
    lib/stdio.c
    lib/stdio_impl.c
    lib/stdlib.c
    lib/string.c
    lib/liballoc.c
)

add_library(mos_kernel.elf STATIC
    ${MOS_STANDARD_LIBRARY_SOURCES}
    kernel/cmdline.c
    kernel/device/console.c
    kernel/device/block.c
    kernel/elf/elf.c
    kernel/filesystem/cpio/cpio.c
    kernel/filesystem/filesystem.c
    kernel/filesystem/mount.c
    kernel/filesystem/pathutils.c
    kernel/ksyscall.c
    kernel/io/io.c
    kernel/io/terminal.c
    kernel/kernel_init.c
    kernel/mm/cow.c
    kernel/mm/memops.c
    kernel/panic.c
    kernel/printk.c
    kernel/stack_protector.c
    kernel/tasks/fork.c
    kernel/tasks/process.c
    kernel/tasks/thread.c
    kernel/tasks/schedule.c
)

generate_syscall_headers(mos_kernel.elf ${CMAKE_SOURCE_DIR}/kernel/ksyscalls.json)

target_link_libraries(mos_kernel.elf PRIVATE gcc mos::basic_include mos::platform_include)
target_compile_definitions(mos_kernel.elf PRIVATE -D__MOS_KERNEL__)
target_compile_options(mos_kernel.elf PUBLIC "-ffreestanding")

add_library(mos::elf_kernel ALIAS mos_kernel.elf)

# Configure architecture-specific stuff
include(add_kernel_source)
include(add_bootable_target)

include(${MOS_ARCH_CONFIG})

add_library(mos_libuserspace OBJECT userspace/libuserspace.c ${MOS_STANDARD_LIBRARY_SOURCES})
target_include_directories(mos_libuserspace PUBLIC ${CMAKE_SOURCE_DIR}/userspace/include)
target_link_libraries(mos_libuserspace PUBLIC gcc mos::basic_include mos::platform_include)
add_library(mos::libuserspace ALIAS mos_libuserspace)

# build userspace programs after the kernel (may require the generated syscall headers)
add_dependencies(mos_libuserspace mos::elf_kernel)

add_subdirectory(userspace)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

generate_kconfig(mos_kernel.elf)

message("")
message("MOS is now configured :)")
message("")

mos_print_summary()
dump_bootable_targets()
