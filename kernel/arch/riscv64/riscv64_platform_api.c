// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/assert.h"
#include "mos/platform/platform.h"

MOS_STUB_IMPL(platform_regs_t *platform_thread_regs(const thread_t *thread))

// Platform Thread / Process APIs
MOS_STUB_IMPL(void platform_context_setup_main_thread(thread_t *thread, ptr_t entry, ptr_t sp, int argc, ptr_t argv, ptr_t envp))
MOS_STUB_IMPL(void platform_context_setup_child_thread(thread_t *thread, thread_entry_t entry, void *arg))
MOS_STUB_IMPL(void platform_context_clone(const thread_t *from, thread_t *to))
MOS_STUB_IMPL(void platform_context_cleanup(thread_t *thread))

// Platform Context Switching APIs

MOS_STUB_IMPL(void platform_switch_to_thread(thread_t *old_thread, thread_t *new_thread, switch_flags_t switch_flags))
MOS_STUB_IMPL(noreturn void platform_return_to_userspace(platform_regs_t *regs))
