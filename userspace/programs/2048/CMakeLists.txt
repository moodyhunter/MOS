# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(2048 main.c)
target_link_libraries(2048 PRIVATE mos::include)
add_to_initrd(TARGET 2048 /games)
