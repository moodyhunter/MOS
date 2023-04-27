# SPDX-License-Identifier: GPL-3.0-or-later

target_sources(stdlib PRIVATE
    libuserspace.c
    libuserspace++.cpp
    stdio.c
    stdlib.c
    cxa.c
    sys-stat.c
    fcntl.c
)

target_include_directories(stdlib SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/userspace/include)

add_subdirectory(argparse)
add_subdirectory(libconfig)
add_subdirectory(readline)
