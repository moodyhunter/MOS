# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(ext4)

add_executable(mount mount.c)
add_to_initrd(TARGET mount /programs)
