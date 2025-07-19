# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(kd main.c)
target_compile_options(kd PRIVATE -static)
target_link_options(kd PRIVATE -static)
target_link_libraries(kd PRIVATE mos::include mos::libreadline_static)
add_to_initrd(TARGET kd /programs/utils)
