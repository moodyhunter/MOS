# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(locks-bench
    main.c
    ../console-helpers.c
)

setup_userspace_program(locks-bench /programs "Benchmarking different locks")
target_link_libraries(locks-bench PRIVATE mos::private_include mos::librpc-client)
