# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(fork)
add_subdirectory(librpc)
add_subdirectory(ipc)
add_subdirectory(cxx)

add_executable(test-launcher test-main.c)
setup_userspace_program(test-launcher /tests "Userspace test launcher")
