// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/cpu/cpu.hpp"

#include "mos/assert.hpp"
#include "mos/platform/platform.hpp"
#include "mos/platform/platform_defs.hpp"
#include "mos/syslog/printk.hpp"
#include "mos/x86/cpu/cpuid.hpp"
#include "mos/x86/tasks/fpu_context.hpp"

#include <mos_string.hpp>

// clang-format off
#define do_static_assert(feat) MOS_STATIC_ASSERT(X86_CPUID_LEAF_ENUM(feat) >= 0);
#define test_ensure_all_leaves_are_supported(feature) do_static_assert(CPU_FEATURE_##feature)
FOR_ALL_CPU_FEATURES(test_ensure_all_leaves_are_supported)
#undef test_ensure_all_leaves_are_supported
#undef do_static_assert
// clang-format on

void x86_cpu_initialise_caps(void)
{
    platform_cpuinfo_t *cpuinfo = &per_cpu(platform_info->cpu)->cpuinfo;
    memzero(cpuinfo, sizeof(platform_cpuinfo_t));

#define impl_fill_leaf(_l, _sl, _r) cpuinfo->cpuid[X86_CPUID_LEAF_ENUM(_l, _sl, _r, _)] = x86_cpuid(_r, _l, _sl);
    FOR_ALL_SUPPORTED_CPUID_LEAF(impl_fill_leaf);
#undef impl_fill_leaf

    MOS_ASSERT_X(cpu_has_feature(CPU_FEATURE_FSGSBASE), "FSGSBASE is required");
    MOS_ASSERT_X(cpu_has_feature(CPU_FEATURE_FXSR), "FXSR is required");
    MOS_ASSERT_X(cpu_has_feature(CPU_FEATURE_SSE), "SSE is required");
    MOS_ASSERT_X(cpu_has_feature(CPU_FEATURE_XSAVE), "XSAVE is required");

    x86_cpu_set_cr4(x86_cpu_get_cr4() | BIT(7) | BIT(11) | BIT(16)); // set CR4.PGE, CR4.FSGSBASE, CR4.UMIP
}

void x86_cpu_setup_xsave_area(void)
{
    pr_dinfo2(x86_startup, "setting up xsave area...");

    reg_t cr0 = x86_cpu_get_cr0();
    cr0 &= ~BIT(2); // clear coprocessor emulation CR0.EM
    cr0 |= BIT(1);
    x86_cpu_set_cr0(cr0);

    reg_t cr4 = x86_cpu_get_cr4();
    cr4 |= BIT(9) | BIT(10) | BIT(18); // set CR4.OSFXSR, CR4.OSXMMEXCPT and CR4.OSXSAVE
    x86_cpu_set_cr4(cr4);

    reg_t xcr0 = XCR0_X87 | XCR0_SSE; // bit 0, 1
    size_t xsave_size = 512;          // X87 + SSE

    // SSE
    xcr0 |= XCR0_SSE; // bit 1
    xsave_size += 64; // XSAVE header

    // AVX
    if (cpu_has_feature(CPU_FEATURE_AVX))
        xcr0 |= XCR0_AVX;

    static const char *const xcr0_names[] = {
        /*0*/ "x87",              //
        /*1*/ "SSE",              //
        /*2*/ "AVX",              //
        /*3*/ "MPX BNDREGS",      //
        /*4*/ "MPX BNDCSR",       //
        /*5*/ "AVX-512 OPMASK",   //
        /*6*/ "AVX-512 ZMM0-15",  //
        /*7*/ "AVX-512 ZMM16-31", //
        /*8*/ "PT",               //
        /*9*/ "PKRU",             //
    };

    for (size_t state_component = 2; state_component < 64; state_component++)
    {
        reg32_t size, offset, ecx, edx;
        __cpuid_count(0xd, state_component, size, offset, ecx, edx);
        if (size && offset && ((ecx & BIT(0)) == 0))
        {
            const char *const name = state_component < MOS_ARRAY_SIZE(xcr0_names) ? xcr0_names[state_component] : "<unknown>";
            pr_dinfo2(x86_startup, "XSAVE state component '%s': size=%d, offset=%d", name, size, offset);

            if (xcr0 & BIT(state_component))
            {
                pr_dcont(x86_startup, " (enabled)");
                xsave_size += size;
            }
        }
    }

    pr_dinfo2(x86_startup, "XSAVE area size: %zu", xsave_size);

    __asm__ volatile("xsetbv" : : "c"(0), "a"(xcr0), "d"(xcr0 >> 32));
    xsave_area_slab.ent_size = xsave_size;
}
