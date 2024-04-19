# SPDX-License-Identifier: GPL-3.0-or-later

target_sources(stdlib_minimal PRIVATE
    libuserspace.c
    libuserspace++.cpp
    liballoc.c
    stdio.c
    stdlib.c
    cxa.c
    sys-stat.c
    fcntl.c
    signal.c
)

target_include_directories(stdlib_minimal SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/userspace/include)

add_subdirectory(argparse)
add_subdirectory(libconfig)
add_subdirectory(readline)

add_to_initrd(TARGET libipc /lib)
add_to_initrd(TARGET librpc-client /lib)
add_to_initrd(TARGET librpc-server /lib)
add_to_initrd(TARGET nanopb /lib)
