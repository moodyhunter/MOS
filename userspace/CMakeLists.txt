# SPDX-License-Identifier: GPL-3.0-or-later

add_custom_target(mos_initrd
    find . -depth | cpio -o --format=crc >../initrd.cpio
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/initrd
    COMMENT "Creating initrd at ${CMAKE_BINARY_DIR}/initrd.cpio"
    BYPRODUCTS ${CMAKE_BINARY_DIR}/initrd.cpio
)

# TODO: Remove this once we have a proper userspace libc
target_compile_options(mos_libuserspace PUBLIC "-ffreestanding")
target_link_options(mos_libuserspace PUBLIC "-nostdlib")

include(cmake/userspace.cmake)

add_subdirectory(libs)
add_subdirectory(init)
add_subdirectory(drivers)
add_subdirectory(tests)

# initrd resources
add_to_initrd(FILE assets/msg.txt /assets)

# if you want to debug a userspace program, replace the line below with the
# target name of the program you want to debug
add_to_gdbinit(init)
