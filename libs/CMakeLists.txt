# SPDX-License-Identifier: GPL-3.0-or-later

# FDT library
add_subdirectory(libfdt++)

# Standard libraries, with some data structures
add_subdirectory(stdlib)

add_subdirectory(nanopb)

# IPC and RPC libraries
add_subdirectory(libipc)
add_subdirectory(librpc)
