# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(ls ls.c)
setup_userspace_program(ls /programs "List files and directories")

add_executable(shutdown shutdown.c)
setup_userspace_program(shutdown /programs "Shutdown the system")
