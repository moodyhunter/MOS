# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(vfs-test main.c)

add_to_initrd(TARGET vfs-test /tests)
