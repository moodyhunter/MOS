# SPDX-License-Identifier: GPL-3.0-or-later

add_userspace_program(x86_console_driver SOURCES
    main.c)

add_initrd_item(TARGET /drivers x86_console_driver)
