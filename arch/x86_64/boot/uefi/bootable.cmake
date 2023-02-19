# SPDX-License-Identifier: GPL-3.0-or-later
include(prepare_bootable_kernel_binary)

add_executable(mos_uefi_entry ${CMAKE_CURRENT_LIST_DIR}/uefi_entry.c)
prepare_bootable_kernel_binary(mos_uefi_entry ${CMAKE_CURRENT_LIST_DIR}/uefi_entry.ld)

