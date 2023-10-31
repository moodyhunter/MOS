# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(init main.c)
target_link_libraries(init
    PRIVATE
        mos::argparse_hosted
        mos::libconfig_hosted
        mos::include
)
add_to_initrd(TARGET init /programs)
