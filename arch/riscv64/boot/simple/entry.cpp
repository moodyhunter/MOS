// SPDX-License-Identifier: GPL-3.0-or-later

#define pr_fmt(fmt) "rv64-startup: " fmt

#include "libfdt++.hpp"
#include "mos/device/console.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/setup.h"

#include <libfdt.h>
#include <mos/device/dm_types.h>
#include <mos/mos_global.h>
#include <mos/types.h>
#include <mos_string.h>

typedef struct
{
    long error;
    long value;
} sbiret_t;

sbiret_t sbi_ecall(int ext, int fid, ulong arg0, ulong arg1, ulong arg2, ulong arg3, ulong arg4, ulong arg5)
{
    sbiret_t ret;

    register ptr_t a0 __asm__("a0") = (ptr_t) (arg0);
    register ptr_t a1 __asm__("a1") = (ptr_t) (arg1);
    register ptr_t a2 __asm__("a2") = (ptr_t) (arg2);
    register ptr_t a3 __asm__("a3") = (ptr_t) (arg3);
    register ptr_t a4 __asm__("a4") = (ptr_t) (arg4);
    register ptr_t a5 __asm__("a5") = (ptr_t) (arg5);
    register ptr_t a6 __asm__("a6") = (ptr_t) (fid);
    register ptr_t a7 __asm__("a7") = (ptr_t) (ext);
    __asm__ volatile("ecall" : "+r"(a0), "+r"(a1) : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7) : "memory");
    ret.error = a0;
    ret.value = a1;

    return ret;
}

// Function Name	            SBI Version	FID	EID
// sbi_debug_console_write	    2	        0	0x4442434E
// sbi_debug_console_read	    2	        1	0x4442434E
// sbi_debug_console_write_byte	2	        2	0x4442434E

#define SBI_DEBUG_CONSOLE_EID 0x4442434E

static standard_color_t sbi_console_bg = Black, sbi_console_fg = White;

should_inline sbiret_t sbi_debug_console_write(ulong num_bytes, ulong base_addr_lo, ulong base_addr_hi)
{
    return sbi_ecall(SBI_DEBUG_CONSOLE_EID, 0x0, num_bytes, base_addr_lo, base_addr_hi, 0, 0, 0);
}

should_inline sbiret_t sbi_debug_console_write_string(const char *str)
{
    return sbi_debug_console_write(strlen(str), (ulong) str, 0);
}

static size_t sbi_console_write(console_t *con, const char *data, size_t size)
{
    MOS_UNUSED(con);
    sbiret_t ret = sbi_debug_console_write(size, (ulong) data, 0);
    return ret.error ? 0 : size;
}

static bool sbi_console_set_color(console_t *con, standard_color_t fg, standard_color_t bg)
{
    MOS_UNUSED(con);
    sbi_console_fg = fg;
    sbi_console_bg = bg;
    char buf[64] = { 0 };
    get_ansi_color(buf, fg, bg);
    sbi_debug_console_write_string(ANSI_COLOR_RESET);
    sbi_debug_console_write_string(buf);
    return true;
}

static bool sbi_console_get_color(console_t *con, standard_color_t *fg, standard_color_t *bg)
{
    MOS_UNUSED(con);
    *fg = sbi_console_fg;
    *bg = sbi_console_bg;
    return true;
}

bool sbi_console_clear(console_t *console)
{
    MOS_UNUSED(console);
    sbi_debug_console_write_string("\033[2J");
    return true;
}

extern void dump_fdt_node(const dt_node &node, int depth = 0);

bool dt_node_status_ok(const dt_node &node)
{
    const auto status = node["status"];
    if (!status)
        return true; // default to "okay"

    const char *status_str = status.get_string();
    if (!strcmp(status_str, "ok") || !strcmp(status_str, "okay"))
        return true;

    return false;
}

static void dt_scan_root(const dt_root &root)
{
    const auto node = root.rootnode();

    const auto size_cells = node["#size-cells"];
    if (size_cells)
        dt_root_size_cells = size_cells.get_u32();

    const auto addr_cells = node["#address-cells"];
    if (addr_cells)
        dt_root_addr_cells = addr_cells.get_u32();

    pr_info2("dt_root_size_cells = %x", dt_root_size_cells);
    pr_info2("dt_root_addr_cells = %x", dt_root_addr_cells);
}

static int dt_check_node(const dt_node &node)
{
    const auto size_cells = node["#size-cells"];
    if (!size_cells)
        return -EINVAL;

    if (size_cells.get_u32() != dt_root_size_cells)
        return -EINVAL;

    const auto addr_cells = node["#address-cells"];
    if (!addr_cells)
        return -EINVAL;

    if (addr_cells.get_u32() != dt_root_addr_cells)
        return -EINVAL;

    const auto ranges = node["ranges"];
    if (!ranges)
        return -EINVAL;

    if (size_cells.get_u32() != dt_root_size_cells || addr_cells.get_u32() != dt_root_addr_cells)
        return -EINVAL;

    return 0;
}

static bool do_reserve_memory(ptr_t base, ptr_t size, bool nomap)
{
    if (size == 0)
        return true;

    MOS_UNUSED(nomap);
    pr_info("early_init_dt_reserve_memory: base " PTR_VLFMT ", size %lu", base, size / (1 KB));
    return true;
}

int dt_scan_reserved_mem(const dt_root &root)
{
    const dt_node reserved_mem = root.get_node("/reserved-memory");

    if (dt_check_node(reserved_mem) != 0)
    {
        pr_warn("reserved memory: unsupported node format, ignoring");
        return -EINVAL;
    }

    for (const auto node : reserved_mem)
    {
        if (!dt_node_status_ok(node))
            continue;

        const bool nomap = node.has_property("no-map");
        const char *uname = node.get_name();

        const dt_reg reg = node["reg"];
        if (!reg.verify_validity())
            mos_panic("reserved memory: invalid reg property in '%s', skipping node.", uname);

        for (const auto &[base, size] : reg)
        {
            if (!do_reserve_memory(base, size, nomap))
                pr_warn("failed to reserve memory for node '%s': base " PTR_VLFMT ", size %lu KiB", uname, base, size / (1 KB));
        }
    }

    return 0;
}

static console_ops_t sbi_console_ops;
static console_t sbi_console;

extern "C" void riscv_simple_main(u64 hart_id, void *fdt)
{
    platform_info->arch_info.fdt = fdt;
    platform_info->cpu.id = hart_id;
    platform_info->boot_cpu_id = hart_id;

    sbi_console_ops.write = sbi_console_write;
    sbi_console_ops.get_color = sbi_console_get_color;
    sbi_console_ops.set_color = sbi_console_set_color;
    sbi_console_ops.clear = sbi_console_clear;

    sbi_console.ops = &sbi_console_ops;
    sbi_console.name = "sbi";
    sbi_console.caps = CONSOLE_CAP_COLOR | CONSOLE_CAP_CLEAR;
    sbi_console.default_fg = White;
    sbi_console.default_bg = Black;

    console_register(&sbi_console);
    pr_emph("riscv64 simple: hart %llu, fdt %p", hart_id, fdt);
    pr_info2("fdt size: %d", fdt_totalsize(fdt));

    // Check the header validity
    if (fdt_check_header(fdt) != 0)
    {
        pr_emerg("invalid FDT header");
        return;
    }

    const dt_root root{ fdt };
    dt_scan_root(root);
    dt_scan_reserved_mem(root);

    for (const auto &node : root.rootnode())
    {
        if (!dt_node_status_ok(node))
            continue;

        const char *uname = node.get_name();
        if (strncmp(uname, "memory@", 7) != 0)
            continue;

        if (!node["reg"])
            continue;

        pr_info("node: %s", uname);
        const dt_reg reg = node["reg"];
        if (!reg.verify_validity())
        {
            pr_warn("invalid reg property in '%s', skipping node.", uname);
            continue;
        }

        for (const auto &[base, size] : reg)
            pr_info("  reg: " PTR_VLRANGE, base, base + size);
    }

    const auto chosen = root.get_node("/chosen");

    const auto initrd_start = chosen["linux,initrd-start"];
    const auto initrd_end = chosen["linux,initrd-end"];

    if (initrd_start && initrd_end)
    {
        pr_info("initrd: " PTR_VLRANGE, (ptr_t) initrd_start.get_u32(), (ptr_t) initrd_end.get_u32());
    }

    const auto bootargs = chosen["bootargs"];
    if (bootargs)
    {
        pr_info("bootargs: %s", bootargs.get_string());
        mos_cmdline_init(bootargs.get_string());
    }

    startup_invoke_earlysetup();

    platform_shutdown();

    *(uint32_t *) 0x100000 = 0x5555;
    mos_start_kernel();
}
