# SPDX-License-Identifier: GPL-3.0-or-later

# x86_64 shares the same platform config as x86, so we just include that

set(MOS_X86_64 ON)
include(${CMAKE_SOURCE_DIR}/arch/x86/platform_config.cmake)
