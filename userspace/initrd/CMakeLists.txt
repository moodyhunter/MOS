# SPDX-License-Identifier: GPL-3.0-or-later

add_to_initrd(DIRECTORY assets  /assets)
add_to_initrd(DIRECTORY config  /config)

make_directory(${CMAKE_BINARY_DIR}/empty)
add_to_initrd(DIRECTORY ${CMAKE_BINARY_DIR}/empty /data)
add_to_initrd(DIRECTORY ${CMAKE_BINARY_DIR}/empty /tmp)

add_to_initrd(FILE README.txt /)

add_to_initrd(FILE "${CMAKE_BINARY_DIR}/summary.txt" /)
