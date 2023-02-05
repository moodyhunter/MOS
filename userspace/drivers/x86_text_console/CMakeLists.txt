# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(x86_console_driver main.c text_mode_console.c)
target_link_libraries(x86_console_driver PRIVATE rpc)
setup_userspace_program(x86_console_driver /drivers "x86 console userspace driver")
