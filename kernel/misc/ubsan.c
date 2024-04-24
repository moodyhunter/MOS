// SPDX-License-Identifier: GPL-3.0-or-later

#define pr_fmt(fmt) "UBSAN: " fmt

#include "mos/printk.h"

#include <mos/types.h>

#undef mos_panic

struct source_location
{
    const char *file;
    u32 line;
    u32 column;
};

struct type_descriptor
{
    u16 kind;
    u16 info;
    char name[];
};

struct type_mismatch_info
{
    struct source_location location;
    struct type_descriptor *type;
    ptr_t alignment;
    u8 type_check_kind;
};

struct out_of_bounds_info
{
    struct source_location location;
    // struct type_descriptor left_type;
    // struct type_descriptor right_type;
};

static void log_location(struct source_location *location)
{
    pr_emerg("  in file %s:%u, column %u", location->file, location->line, location->column);
}

void __ubsan_handle_type_mismatch(struct type_mismatch_info *type_mismatch, ptr_t pointer)
{
    struct source_location *location = &type_mismatch->location;
    if (pointer == 0)
    {
        pr_emerg("NULL pointer access");
    }
    else if (type_mismatch->alignment != 0 && is_aligned(pointer, type_mismatch->alignment))
    {
        // Most useful on architectures with stricter memory alignment requirements, like ARM.
        pr_emerg("unaligned memory access");
    }
    else
    {
        pr_emerg("insufficient size");
        pr_emerg("kind=%d, address %p with insufficient space for object of type %s", type_mismatch->type_check_kind, (void *) pointer, type_mismatch->type->name);
    }
    log_location(location);
}

void __ubsan_handle_pointer_overflow(struct source_location *location, ptr_t pointer)
{
    pr_emerg("pointer overflow, pointer=%p", (void *) pointer);
    log_location(location);
}

void __ubsan_handle_type_mismatch_v1(struct type_mismatch_info *type_mismatch, ptr_t pointer)
{
    __ubsan_handle_type_mismatch(type_mismatch, pointer);
}

void __ubsan_handle_divrem_overflow(struct source_location *location, struct type_descriptor *type, ptr_t left, ptr_t right)
{
    pr_emerg("division overflow, left=%p, right=%p of type %s", (void *) left, (void *) right, type->name);
    log_location(location);
}

void __ubsan_handle_mul_overflow(struct source_location *location, struct type_descriptor *type, ptr_t left, ptr_t right)
{
    pr_emerg("multiplication overflow, left=%p, right=%p of type %s", (void *) left, (void *) right, type->name);
    log_location(location);
}

void __ubsan_handle_add_overflow(struct source_location *location, struct type_descriptor *type, ptr_t left, ptr_t right)
{
    pr_emerg("addition overflow, left=%p, right=%p of type %s", (void *) left, (void *) right, type->name);
    log_location(location);
}

void __ubsan_handle_sub_overflow(struct source_location *location, struct type_descriptor *type, ptr_t left, ptr_t right)
{
    pr_emerg("subtraction overflow, left=%p, right=%p of type %s", (void *) left, (void *) right, type->name);
    log_location(location);
}

void __ubsan_handle_out_of_bounds(struct out_of_bounds_info *out_of_bounds)
{
    struct source_location *location = &out_of_bounds->location;
    pr_emerg("out of bounds");
    log_location(location);
}

void __ubsan_handle_negate_overflow(struct source_location *location, struct type_descriptor *type, ptr_t old_value)
{
    pr_emerg("negate overflow, old value %p of type %s", (void *) old_value, type->name);
    log_location(location);
}

void __ubsan_handle_negate_overflow_v1(struct source_location *location, struct type_descriptor *type, ptr_t old_value)
{
    __ubsan_handle_negate_overflow(location, type, old_value);
}

void __ubsan_handle_load_invalid_value(struct source_location *location, struct type_descriptor *type, ptr_t value)
{
    pr_emerg("load invalid value at address %p, value %p of type %s", (void *) location, (void *) value, type->name);
    log_location(location);
}

void __ubsan_handle_shift_out_of_bounds(struct source_location *location, struct type_descriptor *lhs_type, ptr_t lhs, struct type_descriptor *rhs_type, ptr_t rhs)
{
    pr_emerg("shift out of bounds, lhs=(%s) %p, rhs=(%s) %p", lhs_type->name, (void *) lhs, rhs_type->name, (void *) rhs);
    log_location(location);
}

void __ubsan_handle_invalid_builtin(struct source_location *location)
{
    pr_emerg("invalid builtin");
    log_location(location);
}

void __ubsan_handle_vla_bound_not_positive(struct source_location *location, struct type_descriptor *type, ptr_t bound)
{
    pr_emerg("VLA bound not positive, bound=%p of type %s", (void *) bound, type->name);
    log_location(location);
}
