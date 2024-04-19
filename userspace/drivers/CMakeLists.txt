# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(acpi-daemon)

add_subdirectory(blockdev-layers)
add_subdirectory(blockdev-manager)

add_subdirectory(device-manager)
add_subdirectory(pci-daemon)

add_subdirectory(libdma)

add_simple_rust_project(
    "${CMAKE_CURRENT_LIST_DIR}/virtiod"
    virtiod
    "/drivers/"
    DEPENDS dma
)
