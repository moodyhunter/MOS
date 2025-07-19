# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(acpi-daemon)

add_subdirectory(ramdisk)
add_subdirectory(pci-daemon)

add_simple_rust_project("./virtiod" virtiod "/drivers/" DEPENDS dma)
