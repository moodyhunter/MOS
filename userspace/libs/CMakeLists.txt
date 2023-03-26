# SPDX-License-Identifier: GPL-3.0-or-later

add_library(libuserspace OBJECT
    libuserspace.c
    libuserspace++.cpp
    cxa.c
)

target_include_directories(libuserspace PUBLIC ${CMAKE_SOURCE_DIR}/userspace/include)
target_link_libraries(libuserspace PUBLIC gcc mos::include mos::stdlib)
add_dependencies(libuserspace mos_syscall_decl) # needs to be generated before we can compile
add_library(mos::libuserspace ALIAS libuserspace)

add_subdirectory(argparse)
add_subdirectory(readline)
