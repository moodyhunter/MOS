# SPDX-License-Identifier: GPL-3.0-or-later

# generic architecture files
add_subdirectory(generic)

# architecture-specific files
add_subdirectory(${MOS_ARCH})
