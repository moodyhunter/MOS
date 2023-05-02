# SPDX-License-Identifier: GPL-3.0-or-later

if (NOT MOS_ARCH STREQUAL "x86")
    return()
endif()

add_executable(screen-hello main.c)
target_link_libraries(screen-hello PRIVATE x86-console::client)
setup_userspace_program(screen-hello /programs "A Hello World for the Screen")

