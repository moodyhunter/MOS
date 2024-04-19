# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(mossh
    main.c
    builtin.c
    utils.c
    cmdline.c
)

target_link_libraries(mossh PRIVATE mos::argparse mos::libreadline)
add_to_initrd(TARGET mossh /programs)
