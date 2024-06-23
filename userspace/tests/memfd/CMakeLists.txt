# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(memfd_test main.cpp)

add_to_initrd(TARGET memfd_test /tests)
