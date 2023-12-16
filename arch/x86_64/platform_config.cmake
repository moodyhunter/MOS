# SPDX-License-Identifier: GPL-3.0-or-later

set(MOS_CX_FLAGS "${MOS_CX_FLAGS} -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx")
set(MOS_KERNEL_CFLAGS "-mno-red-zone;-mcmodel=kernel")
set(MOS_BITS 64)
