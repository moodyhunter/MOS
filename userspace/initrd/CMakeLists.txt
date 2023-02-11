# SPDX-License-Identifier: GPL-3.0-or-later

make_directory(${CMAKE_CURRENT_LIST_DIR}/empty)

add_to_initrd(DIRECTORY assets  /assets)
add_to_initrd(DIRECTORY config  /config)

add_to_initrd(DIRECTORY empty /data)
add_to_initrd(DIRECTORY empty /tmp)

add_to_initrd(FILE README.txt /)
