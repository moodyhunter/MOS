# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Please create a separate build directory and run CMake from there.")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(mos_target_setup)

project(MOS
    LANGUAGES CXX C ASM ASM_NASM
    VERSION 0.1
    DESCRIPTION "MOS Kernel and Userspace"
    HOMEPAGE_URL "https://github.com/moodyhunter/MOS"
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# ! Kconfig ===================================================
execute_process(
    COMMAND git describe --long --tags --all --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE REVISION
    ERROR_VARIABLE _DROP_
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)

if (NOT MOS_CONFIG)
    set(MOS_CONFIG ${MOS_ARCH}-default)
    message("Using default config: ${MOS_CONFIG}")
endif()

set(KCONFIG_ARCH ${MOS_ARCH})
set(KCONFIG_PRESET_CONFIG ${CMAKE_SOURCE_DIR}/configs/${MOS_CONFIG}.conf)
set(KCONFIG_PREFIX "MOS_") # use MOS_ prefix for all config variables
set(KCONFIG_EXTRA_OPTIONS_BLACKLIST "MOS_CONFIG;MOS_ARCH;MOS_IS_TOP_LEVEL;MOS_BINARY_DIR;MOS_DESCRIPTION;MOS_SOURCE_DIR;MOS_VERSION")
include(kconfig)

# ! Configure Summary =========================================

include(configure_summary)
add_summary_section(BOOTABLE    "Bootable Targets")
add_summary_section(INITRD      "Initrd Items")
add_summary_section(USERSPACE   "Userspace Targets")
add_summary_section(UTILITY     "Utility Targets")

include(generate_syscall_headers)

# mlibc headers
if (NOT EXISTS ${MOS_MLIBC_HEADER_DIR}/mlibc-config.h)
    message(WARNING "mlibc-config.h not found in ${MOS_MLIBC_HEADER_DIR}. Please verify that mlibc is installed and that MOS_MLIBC_HEADER_DIR is set correctly.")
endif()

# Public includes interface target
add_library(mos_kernel_include INTERFACE)
target_include_directories(mos_kernel_include SYSTEM INTERFACE
    ${MOS_MLIBC_HEADER_DIR}/                            # mlibc headers
    ${CMAKE_BINARY_DIR}/include                         # generated headers
    ${CMAKE_BINARY_DIR}/kconfig/include                 # generated kconfig headers
    ${CMAKE_SOURCE_DIR}/kernel/include/public           # public types
    ${CMAKE_SOURCE_DIR}/arch/${MOS_ARCH}/include/public # public arch (e.g. syscall)
)
add_library(mos::include ALIAS mos_kernel_include)

# Private includes interface target
add_library(mos_kernel_private_include INTERFACE)
target_include_directories(mos_kernel_private_include INTERFACE
    ${CMAKE_SOURCE_DIR}/kernel/include/private
    ${CMAKE_SOURCE_DIR}/arch/${MOS_ARCH}/include/private
)
add_library(mos::private_include ALIAS mos_kernel_private_include)

add_library(mos_kernel_compile_options INTERFACE)
target_compile_options(mos_kernel_compile_options INTERFACE -ffreestanding ${MOS_KERNEL_CFLAGS})
target_compile_definitions(mos_kernel_compile_options INTERFACE -D__MOS_KERNEL__ -D__MLIBC_ABI_ONLY)
target_link_options(mos_kernel_compile_options INTERFACE -nostdlib)
target_link_libraries(mos_kernel_compile_options INTERFACE mos::include mos::private_include gcc)
add_library(mos::kernel_compile_options ALIAS mos_kernel_compile_options)

# The Kernel
file(GLOB_RECURSE MOS_KERNEL_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/kernel/*.c)
add_library(mos_kernel OBJECT ${MOS_KERNEL_SOURCES})
target_link_libraries(mos_kernel PUBLIC mos::kernel_compile_options)
add_dependencies(mos_kernel mos_syscall_decl)
add_library(mos::kernel ALIAS mos_kernel)
target_compile_definitions(mos_kernel PRIVATE MOS_KERNEL_VERSION="${CMAKE_PROJECT_VERSION}" MOS_KERNEL_REVISION="${REVISION}")

# Libraries
include(add_kernel_source)
include(add_mos_library)
add_subdirectory(libs)

# Configure architecture-specific stuff
include(create_bootable_kernel_binary)
add_subdirectory(arch/${MOS_ARCH})

add_subdirectory(userspace)
add_subdirectory(tests)
add_subdirectory(tools)

# include custom configuration
if(EXISTS ${CMAKE_SOURCE_DIR}/config.cmake)
    message(STATUS "Importing custom configuration from ${CMAKE_SOURCE_DIR}/config.cmake")
    include(${CMAKE_SOURCE_DIR}/config.cmake)
endif()

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
    FILES ${KCONFIG_AUTOCONF_H}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/generated/
    COMPONENT mos-api-headers
    EXCLUDE_FROM_ALL
)

install(
    FILES ${MOS_SYSCALL_FILES}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/mos/syscall
    COMPONENT mos-api-headers
    EXCLUDE_FROM_ALL
)

add_custom_target(mos_print_summary COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_BINARY_DIR}/summary.txt USES_TERMINAL)
add_summary_item(UTILITY "mos_print_summary" "" "Print Configuration Summary")

message("")
message("MOS is now configured :)")
message("")

print_summary()
