# SPDX-License-Identifier: GPL-3.0-or-later

# system call headers (generated from ksyscalls.json)
set(MOS_SYSCALL_DIR ${CMAKE_BINARY_DIR}/include/mos/syscall)
set(MOS_SYSCALL_FILES
    ${MOS_SYSCALL_DIR}/decl.h
    ${MOS_SYSCALL_DIR}/dispatcher.h
    ${MOS_SYSCALL_DIR}/number.h
    ${MOS_SYSCALL_DIR}/usermode.h
    ${MOS_SYSCALL_DIR}/table.h
)

find_program(PYTHON "python3" NAMES "python3 python" REQUIRED)
make_directory("${MOS_SYSCALL_DIR}")

set(MOS_SYSCALL_JSON ${CMAKE_SOURCE_DIR}/kernel/ksyscalls.json)
add_custom_command(
    OUTPUT ${MOS_SYSCALL_FILES}
    MAIN_DEPENDENCY ${MOS_SYSCALL_JSON}
    DEPENDS ${CMAKE_SOURCE_DIR}/scripts/gen_syscall.py
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/scripts/gen_syscall.py ${MOS_SYSCALL_JSON} ${MOS_SYSCALL_DIR}
    VERBATIM
)

add_custom_target(mos_syscall_decl DEPENDS ${MOS_SYSCALL_FILES})
add_summary_item(UTILITY mos_syscall_decl "" "Generate syscall headers")

install(
    FILES ${MOS_SYSCALL_FILES}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/mos/syscall
    COMPONENT mos-api-headers
    EXCLUDE_FROM_ALL
)

# interface libraries for kernel includes
add_library(kernel_public_include INTERFACE)
add_library(mos::include ALIAS kernel_public_include)

# interface libraries for private kernel includes
add_library(kernel_private_include INTERFACE)
add_library(mos::private_include ALIAS kernel_private_include)

# interface libraries for kernel compile options
add_library(kernel_cxflags INTERFACE)
add_library(mos::kernel_compile_options ALIAS kernel_cxflags)

# public includes
target_include_directories(kernel_public_include SYSTEM INTERFACE
    ${CMAKE_BINARY_DIR}/include                         # generated headers
    ${CMAKE_BINARY_DIR}/kconfig/include                 # generated kconfig headers
    ${CMAKE_SOURCE_DIR}/kernel/include/public           # public types
    ${CMAKE_SOURCE_DIR}/kernel/arch/${MOS_ARCH}/include/public # public arch (e.g. syscall)
    ${MOS_MLIBC_HEADER_DIR}/                            # mlibc headers
)

# private includes
target_include_directories(kernel_private_include INTERFACE
    ${CMAKE_SOURCE_DIR}/kernel/include/private
    ${CMAKE_SOURCE_DIR}/kernel/arch/${MOS_ARCH}/include/private
)

target_compile_options(kernel_cxflags INTERFACE $<$<COMPILE_LANGUAGE:C>:${MOS_KERNEL_CFLAGS}>)
target_compile_options(kernel_cxflags INTERFACE $<$<COMPILE_LANGUAGE:CXX>:${MOS_KERNEL_CXXFLAGS}>)
target_compile_definitions(kernel_cxflags INTERFACE -D__MOS_KERNEL__ -D__MLIBC_ABI_ONLY)
target_link_options(kernel_cxflags INTERFACE -nostdlib)
target_link_libraries(kernel_cxflags INTERFACE mos::include mos::private_include gcc)

if ("${MOS_UBSAN}" STREQUAL "y")
    target_compile_options(kernel_cxflags INTERFACE -g -fsanitize=undefined)
    target_link_options(kernel_cxflags INTERFACE -g -fsanitize=undefined)
endif()

# The Kernel
add_library(mos_kernel OBJECT ${MOS_SYSCALL_FILES})
target_link_libraries(mos_kernel PUBLIC mos::kernel_compile_options)
add_dependencies(mos_kernel mos_syscall_decl)
add_library(mos::kernel ALIAS mos_kernel)
target_compile_definitions(mos_kernel PRIVATE MOS_KERNEL_VERSION="${CMAKE_PROJECT_VERSION}" MOS_KERNEL_REVISION="${REVISION}" MOS_ARCH="${MOS_ARCH}")

add_subdirectory(arch)

function(add_kernel_submodule SUBMODULE)
    file(GLOB_RECURSE SUBMODULE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/${SUBMODULE}/*.c ${CMAKE_CURRENT_SOURCE_DIR}/${SUBMODULE}/*.cpp)
    add_kernel_source(${SUBMODULE_SOURCES})
endfunction()

add_kernel_submodule(device)
add_kernel_submodule(filesystem)
add_kernel_submodule(interrupt)
add_kernel_submodule(io)
add_kernel_submodule(ipc)
add_kernel_submodule(lib)
add_kernel_submodule(misc)
add_kernel_submodule(mm)
add_kernel_submodule(syslog)
add_kernel_submodule(tasks)

add_kernel_source(kmain.c ksyscall_entry.c ksyscall.c)

# nanopb
generate_nanopb_proto(FILESYSTEM_PROTO_SRCS FILESYSTEM_PROTO_HEADERS ${CMAKE_SOURCE_DIR}/proto/mos_rpc.proto ${CMAKE_SOURCE_DIR}/proto/filesystem.proto)
add_kernel_source(${FILESYSTEM_PROTO_SRCS} ${FILESYSTEM_PROTO_HEADERS})

add_subdirectory(tests)
