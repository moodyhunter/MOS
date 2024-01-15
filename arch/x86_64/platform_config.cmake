# SPDX-License-Identifier: GPL-3.0-or-later

set(MOS_GLOBAL_C_CXX_FLAGS "${MOS_GLOBAL_C_CXX_FLAGS}")

set(_MOS_KERNEL_FLAGS "-mno-red-zone;-mcmodel=kernel;-mno-sse;-mno-mmx;-mno-sse2;-mno-3dnow;-mno-avx")
list(APPEND MOS_KERNEL_CFLAGS   "${_MOS_KERNEL_FLAGS}")
set(_MOS_KERNEL_FLAGS)
