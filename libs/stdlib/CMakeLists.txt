# SPDX-License-Identifier: GPL-3.0-or-later

add_mos_library(
    NAME stdlib
    PUBLIC_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}/include
    SOURCES
        mos_stdio.cpp
        mos_stdio_impl.cpp
        mos_stdlib.cpp
        mos_string.cpp
)

# special kernel extensions for vsnprintf
target_sources(stdlib_kernel PRIVATE stdio_kernel_ext.cpp)

# needs to be generated before we can compile
add_dependencies(stdlib_minimal mos_syscall_decl)
