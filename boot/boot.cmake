# SPDX-License-Identifier: GPL-3.0-or-later

include(mos_add_bootable)

set(MOS_BOOTABLE_TARGETS)
set(MOS_BOOTABLE_TARGETS_FILE)

mos_add_bootable(multiboot)
mos_add_bootable(multiboot_iso)

# dump the bootable targets into bootables.json
set(BOOTABLE_DATA "{}")
foreach(target ${MOS_BOOTABLE_TARGETS})
    string(JSON BOOTABLE_DATA SET ${BOOTABLE_DATA} "${target}" \"${MOS_BOOTABLE_TARGETS_FILE_${target}}\")
endforeach()
file(WRITE ${CMAKE_BINARY_DIR}/bootables.json "${BOOTABLE_DATA}")
