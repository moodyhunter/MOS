// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#define _EMIT_KMODINFO(name, type, valueVar, value)                                                                                                                      \
    MOS_PUT_IN_SECTION(".mos.modinfo", mos::kmods::KernelModuleInfo, __kmod##name##_##type,                                                                              \
                       {                                                                                                                                                 \
                           .mod_info = mos::kmods::KernelModuleInfo::type,                                                                                               \
                           .valueVar = value,                                                                                                                            \
                       })

#define KMOD_NAME(name)        _EMIT_KMODINFO(_name, MOD_NAME, string, name)
#define KMOD_DESCRIPTION(desc) _EMIT_KMODINFO(_description, MOD_DESCRIPTION, string, desc)
#define KMOD_AUTHOR(author)    _EMIT_KMODINFO(_author, MOD_AUTHOR, string, author)
#define KMOD_ENTRYPOINT(func)  _EMIT_KMODINFO(_entrypoint, MOD_ENTRYPOINT, entrypoint, func)

namespace mos::kmods
{
    using EntryPointType = void (*)(void);
    struct KernelModuleInfo
    {
        enum ModInfo
        {
            _INVALID_,
            MOD_ENTRYPOINT,
            MOD_NAME,
            MOD_AUTHOR,
            MOD_DESCRIPTION,
        } mod_info;
        union
        {
            const char *string;
            EntryPointType entrypoint;
        };
    };
} // namespace mos::kmods
