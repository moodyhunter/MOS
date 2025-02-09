// SPDX-License-Identifier: GPL-3.0-or-later

#define pr_fmt(fmt) "UBSAN: " fmt

#include "mos/syslog/printk.hpp"

#include <mos/types.hpp>

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
    source_location location;
    type_descriptor *type;
    ptr_t alignment;
    u8 type_check_kind;
};

struct out_of_bounds_info
{
    source_location location;
    type_descriptor *array_type;
    type_descriptor *index_type;
};

struct unreachable_data
{
    source_location location;
};

static void log_location(source_location *location)
{
    pr_emerg("  in file %s:%u, column %u", location->file, location->line, location->column);
}

MOSAPI void __ubsan_handle_type_mismatch(type_mismatch_info *type_mismatch, ptr_t pointer)
{
    source_location *location = &type_mismatch->location;
    pr_emerg();
    if (pointer == 0)
        pr_emerg("NULL pointer access");
    else if (type_mismatch->alignment != 0 && is_aligned(pointer, type_mismatch->alignment))
        pr_emerg("unaligned memory access"); // Most useful on architectures with stricter memory alignment requirements, like ARM.
    else
        pr_emerg("insufficient size");
    pr_emerg("kind=%d, address %p, for object of type %s", type_mismatch->type_check_kind, (void *) pointer, type_mismatch->type->name);
    log_location(location);
}

MOSAPI void __ubsan_handle_pointer_overflow(source_location *location, ptr_t pointer)
{
    pr_emerg("pointer overflow, pointer=%p", (void *) pointer);
    log_location(location);
}

MOSAPI void __ubsan_handle_type_mismatch_v1(type_mismatch_info *type_mismatch, ptr_t pointer)
{
    __ubsan_handle_type_mismatch(type_mismatch, pointer);
}

MOSAPI void __ubsan_handle_divrem_overflow(source_location *location, type_descriptor *type, ptr_t left, ptr_t right)
{
    pr_emerg("division overflow, left=%p, right=%p of type %s", (void *) left, (void *) right, type->name);
    log_location(location);
}

MOSAPI void __ubsan_handle_mul_overflow(source_location *location, type_descriptor *type, ptr_t left, ptr_t right)
{
    pr_emerg("multiplication overflow, left=%p, right=%p of type %s", (void *) left, (void *) right, type->name);
    log_location(location);
}

MOSAPI void __ubsan_handle_add_overflow(source_location *location, type_descriptor *type, ptr_t left, ptr_t right)
{
    pr_emerg("addition overflow, left=%p, right=%p of type %s", (void *) left, (void *) right, type->name);
    log_location(location);
}

MOSAPI void __ubsan_handle_sub_overflow(source_location *location, type_descriptor *type, ptr_t left, ptr_t right)
{
    pr_emerg("subtraction overflow, left=%p, right=%p of type %s", (void *) left, (void *) right, type->name);
    log_location(location);
}

MOSAPI void __ubsan_handle_out_of_bounds(out_of_bounds_info *out_of_bounds)
{
    source_location *location = &out_of_bounds->location;
    pr_emerg("out of bounds, array type %s, index type %s", out_of_bounds->array_type->name, out_of_bounds->index_type->name);
    log_location(location);
}

MOSAPI void __ubsan_handle_negate_overflow(source_location *location, type_descriptor *type, ptr_t old_value)
{
    pr_emerg("negate overflow, old value %p of type %s", (void *) old_value, type->name);
    log_location(location);
}

MOSAPI void __ubsan_handle_negate_overflow_v1(source_location *location, type_descriptor *type, ptr_t old_value)
{
    __ubsan_handle_negate_overflow(location, type, old_value);
}

MOSAPI void __ubsan_handle_load_invalid_value(void *data, ptr_t value)
{
    pr_emerg("load invalid value at %p of value %llu", data, value);
}

MOSAPI void __ubsan_handle_shift_out_of_bounds(source_location *location, type_descriptor *lhs_type, ptr_t lhs, type_descriptor *rhs_type, ptr_t rhs)
{
    pr_emerg("shift out of bounds, lhs=(%s) %p, rhs=(%s) %p", lhs_type->name, (void *) lhs, rhs_type->name, (void *) rhs);
    log_location(location);
}

MOSAPI void __ubsan_handle_invalid_builtin(source_location *location)
{
    pr_emerg("invalid builtin");
    log_location(location);
}

MOSAPI void __ubsan_handle_vla_bound_not_positive(source_location *location, type_descriptor *type, ptr_t bound)
{
    pr_emerg("VLA bound not positive, bound=%p of type %s", (void *) bound, type->name);
    log_location(location);
}

MOSAPI void __ubsan_handle_builtin_unreachable(unreachable_data *data)
{
    pr_emerg("builtin unreachable was reached");
    log_location(&data->location);
}
