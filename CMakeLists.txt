# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/kconfig.cmake)
include(cmake/mos_target_setup.cmake)
include(mos_config.cmake)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Please create a separate build directory and run CMake from there.")
endif()

project(MOS LANGUAGES CXX C ASM_NASM VERSION 0.1)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(GNUInstallDirs)

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

include(generate_syscall_headers)
generate_syscall_headers(${CMAKE_SOURCE_DIR}/kernel/ksyscalls.json)

# Public includes interface target
add_library(mos_kernel_include INTERFACE)
target_include_directories(mos_kernel_include SYSTEM INTERFACE
    ${CMAKE_BINARY_DIR}/include                         # generated headers
    ${CMAKE_SOURCE_DIR}/kernel/include/public           # public types
    ${CMAKE_SOURCE_DIR}/arch/${MOS_ARCH}/include/public # public arch (e.g. syscall)
)
add_library(mos::include ALIAS mos_kernel_include)

# Private includes interface target
add_library(mos_kernel_private_include INTERFACE)
target_include_directories(mos_kernel_private_include SYSTEM INTERFACE
    ${CMAKE_SOURCE_DIR}/kernel/include/private
    ${CMAKE_SOURCE_DIR}/arch/${MOS_ARCH}/include/private
)
add_library(mos::private_include ALIAS mos_kernel_private_include)

# The Kernel
file(GLOB_RECURSE MOS_KERNEL_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/kernel/*.c)
add_library(mos_kernel OBJECT ${MOS_KERNEL_SOURCES})
target_link_libraries(mos_kernel PUBLIC gcc mos::include)
target_link_libraries(mos_kernel PRIVATE mos::private_include)
target_compile_definitions(mos_kernel PRIVATE -D__MOS_KERNEL__)
target_compile_options(mos_kernel PUBLIC "-ffreestanding")
add_dependencies(mos_kernel mos_syscall_decl)
add_library(mos::kernel ALIAS mos_kernel)

# Libraries
include(add_kernel_source)
include(add_mos_library)
add_subdirectory(libs)

# Configure architecture-specific stuff
include(add_bootable_target)
include(prepare_bootable_kernel_binary)
include(${MOS_ARCH_CONFIG})

add_subdirectory(userspace)
add_subdirectory(tests)
add_subdirectory(tools)

# include custom configuration
if(EXISTS ${CMAKE_SOURCE_DIR}/config.cmake)
    message(STATUS "Including custom configuration from ${CMAKE_SOURCE_DIR}/config.cmake")
    include(${CMAKE_SOURCE_DIR}/config.cmake)
endif()

generate_kconfig()

# Configure installation targets
install(
    DIRECTORY
        ${CMAKE_SOURCE_DIR}/kernel/include/public/mos
        ${CMAKE_SOURCE_DIR}/arch/${MOS_ARCH}/include/public/mos
    TYPE INCLUDE
    COMPONENT mos-api-headers
    EXCLUDE_FROM_ALL
)

# We cannot just install the ${CMAKE_BINARY_DIR}/include because it contains
# the generated syscall headers, which MAY NOT be generated at the time of
# installation.
# So we explicitly list the files we want to install here.
install(
    FILES ${CMAKE_BINARY_DIR}/include/mos/kconfig.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mos
    COMPONENT mos-api-headers
    EXCLUDE_FROM_ALL
)

install(
    FILES ${MOS_SYSCALL_FILES}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mos/syscalls
    COMPONENT mos-api-headers
    EXCLUDE_FROM_ALL
)

add_custom_target(mos_print_summary COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_BINARY_DIR}/summary.txt USES_TERMINAL)
add_summary_item(UTILITY "mos_print_summary" "" "Print Configuration Summary")

message("")
message("MOS is now configured :)")
message("")

print_summary()
dump_bootable_targets()
