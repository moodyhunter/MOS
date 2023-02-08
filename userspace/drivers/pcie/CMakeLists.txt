# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(pcie-driver main.c)
setup_userspace_program(pcie-driver /drivers "PCIe Bus Driver")
