# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(fork-test fork_test.c)
target_link_libraries(fork-test PRIVATE mos::libuserspace)
add_initrd_item(TARGET /tests/ fork-test)
