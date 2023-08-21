# SPDX-License-Identifier: GPL-3.0-or-later

if (NOT MOS_ARCH STREQUAL "x86_64")
    return()
endif()

add_executable(pci_daemon
    main.c
    pci_scan.c
    known_devices.c
)

target_link_libraries(pci_daemon PRIVATE mos::stdlib mos::private_include librpc::client)
target_link_libraries(pci_daemon PRIVATE device-manager::client)

setup_userspace_program(pci_daemon /drivers "x86 PCI driver")
