// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/tasks/context.h"

#include "lib/string.h"
#include "lib/structures/stack.h"
#include "mos/mm/kmalloc.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/task_type.h"
#include "mos/x86/x86_platform.h"

extern void x86_context_switch_impl(x86_context_t *old, x86_context_t *new);

void x86_thread_context_init(thread_t *t, thread_entry_t entry, void *arg)
{
    MOS_UNUSED(arg);
    x86_context_t *ctx = kmalloc(sizeof(x86_context_t));
    memset(ctx, 0, sizeof(x86_context_t));
    t->context = get_common_context_t(ctx);

    if (t->flags & THREAD_FLAG_USERMODE)
    {
        ctx->cs = GDT_SEGMENT_USERCODE | 0x03; // user code segment + ring 3
        ctx->ds = GDT_SEGMENT_USERDATA | 0x03; // user data segment + ring 3
        ctx->es = GDT_SEGMENT_USERDATA | 0x03; // user data segment + ring 3
        ctx->fs = GDT_SEGMENT_USERDATA | 0x03; // user data segment + ring 3
        ctx->gs = GDT_SEGMENT_USERDATA | 0x03; // user data segment + ring 3
        ctx->ss = GDT_SEGMENT_USERDATA | 0x03; // user data segment + ring 3
    }
    else if (t->flags & THREAD_FLAG_KTHREAD)
    {
        ctx->cs = GDT_SEGMENT_KCODE | 0x00; // kernel code segment + ring 0
        ctx->ds = GDT_SEGMENT_KDATA | 0x00; // kernel data segment + ring 0
        ctx->es = GDT_SEGMENT_KDATA | 0x00; // kernel data segment + ring 0
        ctx->fs = GDT_SEGMENT_KDATA | 0x00; // kernel data segment + ring 0
        ctx->gs = GDT_SEGMENT_KDATA | 0x00; // kernel data segment + ring 0
        ctx->ss = GDT_SEGMENT_KDATA | 0x00; // kernel data segment + ring 0
    }
    else
    {
        mos_panic("Unknown thread flags: %d", t->flags);
    }

    reg_t esi = 0;
    reg_t edi = 0;
    reg_t ebx = 0;
    reg_t ebp = 0;

    stack_push(&t->stack, &ctx->cs, sizeof(reg_t));
    stack_push(&t->stack, &entry, sizeof(reg_t));

    reg_t *esp = stack_grow(&t->stack, sizeof(reg_t));
    stack_push(&t->stack, &ebp, sizeof(reg_t));
    stack_push(&t->stack, &ebx, sizeof(reg_t));
    stack_push(&t->stack, &edi, sizeof(reg_t));
    stack_push(&t->stack, &esi, sizeof(reg_t));
    *esp = (reg_t) t->stack.head;

    // pop esi
    // pop edi
    // pop ebx
    // pop ebp
}

void x86_context_switch(thread_t *from, thread_t *to)
{
    x86_context_t *from_ctx = get_context_t(from->context, x86_context_t);
    x86_context_t *to_ctx = get_context_t(to->context, x86_context_t);
    pr_info("context switch: %d: " PTR_FMT " -> %d: " PTR_FMT, from->id.thread_id, (uintptr_t) from_ctx, to->id.thread_id, (uintptr_t) to_ctx);
    x86_context_switch_impl(from_ctx, to_ctx);
}

void x86_context_jump(thread_t *t)
{
    x86_context_t *ctx = get_context_t(t->context, x86_context_t);
    x86_context_switch_impl(NULL, ctx);
}
