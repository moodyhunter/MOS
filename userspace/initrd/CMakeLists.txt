# SPDX-License-Identifier: GPL-3.0-or-later

add_to_initrd(DIRECTORY assets  /assets)
add_to_initrd(DIRECTORY config  /config)
add_to_initrd(DIRECTORY etc  /etc)
add_to_initrd(FILE README.txt /)
add_to_initrd(FILE "${CMAKE_BINARY_DIR}/summary.txt" /)
