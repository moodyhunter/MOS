# SPDX-License-Identifier: GPL-3.0-or-later

if (NOT MOS_ARCH STREQUAL "x86_64")
    message(WARNING "PCI daemon is only supported on x86_64")
    return()
endif()

add_executable(pci-daemon
    main.c
    pci_scan.c
    known_devices.c
)

target_link_libraries(pci-daemon PRIVATE librpc::client_hosted)
target_link_libraries(pci-daemon PRIVATE device-manager::client)

add_to_initrd(TARGET pci-daemon /drivers)
