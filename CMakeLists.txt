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
set(MOS_ARCH_CONFIG ${CMAKE_SOURCE_DIR}/arch/${MOS_ARCH}/mos_platform.cmake)

if(NOT EXISTS ${MOS_ARCH_CONFIG})
    message(FATAL_ERROR "No configuration file for architecture '${MOS_ARCH}'")
endif()

include(generate_syscall_headers)

add_library(mos_kernel_include INTERFACE)
target_include_directories(mos_kernel_include INTERFACE
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_SOURCE_DIR}/kernel/include
    ${CMAKE_SOURCE_DIR}/lib/include
    ${CMAKE_SOURCE_DIR}/arch/${MOS_ARCH}/include
)
add_library(mos::include ALIAS mos_kernel_include)

execute_process(
    COMMAND git describe --long --tags --all --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE REVISION
    ERROR_VARIABLE _DROP_
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)

mos_kconfig(MISC MOS_KERNEL_VERSION "\"${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}\"" "Kernel version")
mos_kconfig(MISC MOS_KERNEL_REVISION_STRING "\"${REVISION}\"" "Kernel revision string")

file(GLOB_RECURSE MOS_STANDARD_LIBRARY_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/lib/*.c)
file(GLOB_RECURSE MOS_KERNEL_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/kernel/*.c)

generate_syscall_headers(${CMAKE_SOURCE_DIR}/kernel/ksyscalls.json)

add_library(mos_kernel STATIC ${MOS_STANDARD_LIBRARY_SOURCES} ${MOS_KERNEL_SOURCES})
target_link_libraries(mos_kernel PRIVATE gcc mos::include mos::syscall::decl)
target_compile_definitions(mos_kernel PRIVATE -D__MOS_KERNEL__)
target_compile_options(mos_kernel PUBLIC "-ffreestanding")
add_library(mos::kernel ALIAS mos_kernel)

# Configure architecture-specific stuff
include(add_kernel_source)
include(add_bootable_target)

include(${MOS_ARCH_CONFIG})

add_subdirectory(userspace)
add_subdirectory(tests)

generate_kconfig(mos_kernel)

message("")
message("MOS is now configured :)")
message("")

mos_print_summary()
dump_bootable_targets()
