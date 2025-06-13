# SPDX-License-Identifier: GPL-3.0-or-later

if (NOT MOS_ARCH STREQUAL "x86_64")
    message(WARNING "PCI daemon is only supported on x86_64")
    return()
endif()

add_executable(pci-daemon
    main.cpp
    pci_scan.cpp
    known_devices.cpp
)

target_link_libraries(pci-daemon PRIVATE librpc::client)
target_link_libraries(pci-daemon PRIVATE device-manager::client)

add_to_initrd(TARGET pci-daemon /drivers)
