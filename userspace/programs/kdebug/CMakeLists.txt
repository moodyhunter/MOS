# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(kdebug main.c)
target_link_libraries(kdebug PRIVATE mos::include mos::libreadline)
add_to_initrd(TARGET kdebug /programs)
