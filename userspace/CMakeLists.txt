# SPDX-License-Identifier: GPL-3.0-or-later

summary_section(USERSPACE "Userspace Programs")
add_custom_target(mos_userspace_programs)
add_custom_target(mos_initrd
    find . -depth | cpio -o --format=crc >../initrd.cpio
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/initrd
    COMMENT "Creating initrd at ${CMAKE_BINARY_DIR}/initrd.cpio"
    BYPRODUCTS ${CMAKE_BINARY_DIR}/initrd.cpio
)

# TODO: Remove this once we have a proper userspace libc
target_compile_options(mos_libuserspace PUBLIC "-ffreestanding")
target_link_options(mos_libuserspace PUBLIC "-nostdlib")

# create a gdbinit file for debugging
# ! GDB doesn't pick correct symbol file when debugging multiple processes, solve this.
write_file(${CMAKE_BINARY_DIR}/gdbinit "# GDB init file for MOS")
write_file(${CMAKE_BINARY_DIR}/gdbinit "" APPEND)

include(cmake/userspace.cmake)

add_subdirectory(libs)

add_subdirectory(init)
add_subdirectory(ping-pong)

add_userspace_program(locks SOURCES locks/main.c)
add_initrd_item(TARGET /programs locks)

add_initrd_item(FILE /assets assets/msg.txt)
