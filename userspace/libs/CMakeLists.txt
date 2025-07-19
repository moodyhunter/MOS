# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(argparse)
add_subdirectory(libdma)
add_subdirectory(libconfig)
add_subdirectory(readline)
add_subdirectory(libsm)

add_to_initrd(TARGET libipc /lib)
add_to_initrd(TARGET librpc-client /lib)
add_to_initrd(TARGET librpc-server /lib)
add_to_initrd(TARGET nanopb /lib)
