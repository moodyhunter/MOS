# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(pcid main.c)
target_link_libraries(pcid PRIVATE mos::stdlib mos::private_include)
setup_userspace_program(pcid /drivers "x86 PCI driver")
