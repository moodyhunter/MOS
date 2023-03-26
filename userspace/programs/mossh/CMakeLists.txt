# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(mossh
    main.c
    builtin.c
    utils.c
)

setup_userspace_program(mossh /programs "MOS Shell")
target_link_libraries(mossh PRIVATE mos::readline mos::argparse)
