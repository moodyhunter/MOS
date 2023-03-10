# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(my_mutex
    main.c
    ../console-helpers.c
)

setup_userspace_program(my_mutex /programs "My own mutex implementation using futexes")
target_link_libraries(my_mutex PRIVATE rpc)
