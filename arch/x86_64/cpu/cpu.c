// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/platform/platform.h"
#include "mos/platform/platform_defs.h"
#include "mos/x86/cpu/cpuid.h"

#include <mos_string.h>

// clang-format off
#define do_static_assert(feat) MOS_STATIC_ASSERT(X86_CPUID_LEAF_ENUM(feat) >= 0);
#define test_ensure_all_leafs_are_supported(feature) do_static_assert(CPU_FEATURE_##feature)
FOR_ALL_CPU_FEATURES(test_ensure_all_leafs_are_supported)
// clang-format on

void x86_cpu_get_caps_all(void)
{
    platform_cpuinfo_t *cpuinfo = &per_cpu(platform_info->cpu)->cpuinfo;
    memzero(cpuinfo, sizeof(platform_cpuinfo_t));

#define impl_fill_leaf(leaf, subleaf, reg) cpuinfo->cpuid[X86_CPUID_LEAF_ENUM(leaf, subleaf, reg, _)] = x86_cpuid(reg, a = leaf, c = subleaf);
    FOR_ALL_SUPPORTED_CPUID_LEAF(impl_fill_leaf);
#undef impl_fill_leaf
}
