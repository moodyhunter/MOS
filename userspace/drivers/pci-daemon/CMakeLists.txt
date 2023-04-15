# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(pci_daemon main.c pci_scan.c)
target_link_libraries(pci_daemon PRIVATE mos::stdlib mos::private_include librpc::client)
setup_userspace_program(pci_daemon /drivers "x86 PCI driver")
