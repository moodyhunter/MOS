// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/dentry.hpp"
#include "mos/misc/kallsyms.hpp"
#include "mos/mm/mm.hpp"
#include "mos/mm/mm_types.hpp"
#include "mos/mm/paging/paging.hpp"
#include "mos/platform/platform.hpp"

#include <initializer_list>
#include <mos/hashmap.hpp>
#include <mos/kmod/kmod-decl.hpp>
#include <mos/mos_global.h>
#include <mos/shared_ptr.hpp>
#include <mos/string.hpp>
#include <mos/vector.hpp>
#include <numeric>
#define __MLIBC_EMIT_INLINE_DEFINITIONS
#include "mos/filesystem/vfs.hpp"
#include "mos/filesystem/vfs_types.hpp"
#include "mos/filesystem/vfs_utils.hpp"
#include "mos/kmod/kmod.hpp"
#include "mos/syslog/debug.hpp"
#include "mos/tasks/elf.hpp"

#include <elf.h>
#include <mos/allocator.hpp>
#include <mos/filesystem/fs_types.h>
#include <mos/mm/mm_types.h>

ptr_t VAlloc(size_t nPages)
{
    static ptr_t NextVaddr = 0xFFFFFFFFC0000000; // Start from the top of the kernel address space
    ptr_t vaddr = NextVaddr;
    NextVaddr += nPages * MOS_PAGE_SIZE;
    const auto pfn = phyframe_pfn(mm_get_free_pages(nPages));
    mm_map_kernel_pages(platform_info->kernel_mm, vaddr, pfn, nPages, VM_RWX);
    return vaddr;
}

using init_function_t = void (*)();

namespace mos::kmods
{
    enum class ElfSectionType
    {
        NULL_ = 0,
        PROGBITS = 1,
        SYMTAB = 2,
        STRTAB = 3,
        RELA = 4,
        HASH = 5,
        DYNAMIC = 6,
        NOTE = 7,
        NOBITS = 8,
        REL = 9,
        DYNSYM = 11,
        INIT_ARRAY = 14,
        FINI_ARRAY = 15,
    };

    struct Section;

    constexpr auto KMODINFO_SECTION_NAME = ".mos.modinfo";

    struct Symbol : public mos::NamedType<"kmod.symbol">
    {
        explicit Symbol() : value(0), size(0), type(Undefined), symbol_type(NoType), binding(Local)
        {
            this->undefined.offset = 0;
        }

        ~Symbol()
        {
            switch (type)
            {
                case Undefined:
                    // No additional cleanup needed for undefined symbols
                    break;
                case Absolute:
                    // No additional cleanup needed for absolute symbols
                    break;
                case Regular:
                    this->regular.section.reset(); // Release the section pointer for regular symbols
                    break;
            }
        }

        mos::string_view name; // symbol name
        uint64_t index;
        u64 value;   // symbol value (address)
        size_t size; // size of the symbol in bytes (0 if not applicable)
        enum
        {
            Undefined,
            Absolute,
            Regular,
        } type = Undefined; // type of the symbol (e.g., undefined, absolute, regular)
        union
        {
            struct
            {
                u64 offset; // offset in the section for undefined symbols
            } undefined;
            struct
            {
                u64 value; // absolute value for absolute symbols
            } absolute;
            struct
            {
                mos::shared_ptr<Section> section; // section this symbol belongs to
            } regular;
        };

        enum
        {
            NoType = 0,
            Object = 1,
            Function = 2,
            SectionSymbol = 3,
            File = 4,
        } symbol_type = NoType; // type of the symbol (e.g., object, function, file)

        enum
        {
            Local = 0,
            Global = 1,
            Weak = 2,
        } binding = Local; // binding of the symbol (e.g., local, global, weak)

        std::optional<ptr_t> ResolveRuntimeAddress();
    };

    struct Section : public mos::NamedType<"kmod.section">
    {
        uint64_t index;         // section index
        mos::string name;       // section name
        uintptr_t load_address; // load address in memory
        off_t file_offset;      // offset in the file where this section starts
        size_t size;            // size of the section in bytes
        VMFlags vmflags;        // memory flags for the section (e.g., readable, writable, executable)
        ElfSectionType type;    // type of the section (e.g., PROGBITS, SYMTAB, etc.)

        mos::vector<mos::shared_ptr<Symbol>> symbols; // symbols in this section
    };

    struct Relocation
    {
        mos::shared_ptr<Section> in_section;
        off_t offset;
        u64 addend; // addend for the relocation
        mos::shared_ptr<Symbol> symbol;
        uint64_t type; // relocation type (e.g., R_X86_64_RELATIVE, R_X86_64_64)

        std::optional<std::pair<ptr_t, u64>> GetRelocationAddrAndValue() const
        {
            ptr_t addr = 0;
            u64 value = 0;

            switch (type)
            {
                case R_X86_64_PLT32:
                case R_X86_64_PC32:
                case R_X86_64_32S:
                case R_X86_64_64: addr = in_section->load_address + offset; break;
                default: mEmerg << "Unhandled relocation type: " << type; break;
            }

            const auto symbolAddress = symbol->ResolveRuntimeAddress();
            if (!symbolAddress.has_value())
            {
                mEmerg << "Failed to resolve symbol address for symbol: " << symbol->name;
                return std::nullopt;
            }

            switch (type)
            {
                case R_X86_64_64: value = *symbolAddress + addend; break;
                case R_X86_64_PC32: value = *symbolAddress + addend - addr; break;
                case R_X86_64_PLT32: value = *symbolAddress + addend - addr; break;
                case R_X86_64_32S: value = *symbolAddress + addend; break;
                default: mEmerg << "Unhandled relocation type: " << type; break;
            }

            return std::make_pair(addr, value);
        }
    };

    struct ModuleELFInfo : public mos::NamedType<"kmod.elf_data">
    {
        elf_header_t header;
        FsBaseFile *file = nullptr; // file this module was loaded from

        EntryPointType entrypoint = nullptr;
        mos::vector<ptr<Symbol>> pltEntries; // number of PLT entries in the module

        mos::vector<init_function_t> init_functions; // init functions to call after loading

        mos::HashMap<mos::string, mos::shared_ptr<Section>> sections;

        mos::vector<KernelModuleInfo> module_info; // module info section

        mos::string GetModuleName() const
        {
            for (const auto &info : module_info)
            {
                if (info.mod_info == KernelModuleInfo::MOD_NAME)
                    return info.string;
            }
            mWarn << "No module name found in module info";
            static mos::string empty_name;
            return empty_name;
        }

        mos::shared_ptr<Section> GetSectionByName(const mos::string &name) const
        {
            for (const auto &[section_name, section] : sections)
            {
                if (section_name == name)
                    return section;
            }
            mWarn << "Section with name '" << name << "' not found";
            return nullptr;
        }

        mos::shared_ptr<Section> GetSectionByIndex(size_t index) const
        {
            for (const auto &[_, section] : sections)
            {
                if (section->index == index)
                    return section;
            }
            mWarn << "Section with index " << index << " not found";
            return nullptr;
        }

        mos::vector<char> shstrtab;
        mos::string_view GetShStrTabEntry(size_t index) const
        {
            if (index >= shstrtab.size())
            {
                mWarn << "Invalid string table index: " << index;
                return "";
            }
            return mos::string_view(shstrtab.data() + index);
        }

        mos::vector<char> strtab; // string table for symbols
        mos::string_view GetStrTabEntry(size_t index) const
        {
            if (index >= strtab.size())
            {
                mWarn << "Invalid symbol name index: " << index;
                return "";
            }
            return mos::string_view(strtab.data() + index);
        }

        mos::vector<mos::shared_ptr<Symbol>> symbols; // symbols in this module
        mos::shared_ptr<Symbol> GetSymbolByIndex(size_t index) const
        {
            for (const auto &symbol : symbols)
            {
                if (symbol->index == index)
                    return symbol;
            }
            mWarn << "Symbol with index " << index << " not found";
            return nullptr;
        }

        mos::vector<Relocation> relocations; // relocations for this module

        long FillData();

        bool EmitPLT() const;

        bool LoadIntoMemory();

        bool PerformRelocation();

        void LoadModuleBasicInfo();

        void DumpInfo() const;

        using callback_t = long(ModuleELFInfo &data, const Elf64_Shdr &sh, u16 id, mos::string_view name);

        long ForEachSection(callback_t callback, std::initializer_list<ElfSectionType> type)
        {
            for (u16 i = 0; i < header.sh.count; i++)
            {
                Elf64_Shdr sh;
                if (!file->pread(&sh, header.sh.entry_size, header.sh_offset + i * header.sh.entry_size))
                {
                    mEmerg << "failed to read section header " << i << " for '" << dentry_name(file->dentry) << "'";
                    return -EIO;
                }

                const auto sh_name = GetShStrTabEntry(sh.sh_name);
                for (const auto &t : type)
                {
                    if (sh.sh_type == static_cast<Elf64_Word>(t))
                        if (const auto ret = callback(*this, sh, i, sh_name); IS_ERR_VALUE(ret))
                            return ret;
                }
            }
            return 0;
        }
    };

    void ModuleELFInfo::LoadModuleBasicInfo()
    {
        {
            const auto section = GetSectionByName(KMODINFO_SECTION_NAME);
            if (!section)
            {
                mWarn << "No section named '" << KMODINFO_SECTION_NAME << "' found in module '" << dentry_name(file->dentry) << "'";
                return;
            }

            for (size_t i = 0; i < section->size / sizeof(KernelModuleInfo); i++)
            {
                const auto *info = (KernelModuleInfo *) (section->load_address + i * sizeof(KernelModuleInfo));
                switch (info->mod_info)
                {
                    case KernelModuleInfo::MOD_ENTRYPOINT: entrypoint = info->entrypoint; break;
                    case KernelModuleInfo::MOD_NAME: break;
                    case KernelModuleInfo::MOD_AUTHOR: break;
                    case KernelModuleInfo::MOD_DESCRIPTION: break;
                    default: mWarn << "Unknown module info type: " << info->mod_info; break;
                }
                module_info.push_back(*info);
            }
        }

        {
            const auto section = GetSectionByName(".init_array");

            // these sections contain pointers to init functions
            for (size_t i = 0; i < section->size / sizeof(init_function_t); i++)
            {
                ptr_t functionPtr = *reinterpret_cast<ptr_t *>(section->load_address + i * sizeof(init_function_t));
                init_functions.push_back(reinterpret_cast<init_function_t>(functionPtr));
            }
        }
    }

    bool ModuleELFInfo::LoadIntoMemory()
    {
        // first order the sections by their permissions
        mos::HashMap<VMFlags, mos::vector<mos::shared_ptr<Section>>> sections_by_flags;
        for (const auto &[_, section] : sections)
        {
            if (section->vmflags == VMFlags() || section->size == 0)
            {
                dInfo<kmod> << "Section '" << section->name << "' has no flags set or size is zero, skipping";
                continue;
            }
            sections_by_flags[section->vmflags].push_back(section);
        }

        const auto order = { VM_RX, VM_RW, VM_READ };

        for (const auto flags : order)
        {
            if (const auto sections = sections_by_flags.find(flags); sections == sections_by_flags.end())
            {
                dInfo<kmod> << "No sections with flags: " << flags;
                continue;
            }
            else
            {
                const auto sectionsList = sections.value();
                const auto totalSize = std::accumulate(sectionsList.begin(), sectionsList.end(), 0ULL, [](u64 acc, auto section) { return acc + section->size; });
                const auto nPages = ALIGN_UP_TO_PAGE(totalSize) / MOS_PAGE_SIZE;

                dInfo<kmod> << "  Total size of sections with flags " << flags << ": " << totalSize << ", number of pages: " << nPages;
                auto load_address = VAlloc(nPages);

                for (const auto &section : sectionsList)
                {
                    section->load_address = load_address;
                    load_address += section->size;

                    if (section->name == ".plt")
                        continue; // we don't load PLT into the memory

                    if (section->type == ElfSectionType::NOBITS)
                    {
                        dInfo<kmod> << "  Section '" << section->name << "' is of type NOBITS, skipping loading into memory";
                        memzero((void *) section->load_address, section->size);
                        dInfo<kmod> << "  Initialized section '" << section->name << "' to zero at address " << (void *) section->load_address;
                        continue;
                    }

                    file->pread((void *) section->load_address, section->size, section->file_offset);
                    dInfo<kmod> << "  Loaded section '" << section->name << "' into memory at address " << (void *) section->load_address;
                }
            }
        }
        return true;
    }

    bool ModuleELFInfo::PerformRelocation()
    {
        for (const auto &rel : relocations)
        {
            const auto relocated = rel.GetRelocationAddrAndValue();
            if (!relocated.has_value())
            {
                mWarn << "Failed to get relocation address and value for relocation in section '" << (rel.in_section ? rel.in_section->name : "unknown") << "'";
                return false;
            }

            const auto [addr, value] = *relocated;
            switch (rel.type)
            {
                case R_X86_64_64: *(reinterpret_cast<u64 *>(addr)) = value; break;                      // type 1
                case R_X86_64_PC32: *(reinterpret_cast<s32 *>(addr)) = static_cast<s32>(value); break;  // type 2
                case R_X86_64_PLT32: *(reinterpret_cast<s32 *>(addr)) = static_cast<s32>(value); break; // type 4
                case R_X86_64_32S: *(reinterpret_cast<s32 *>(addr)) = static_cast<s32>(value); break;   // type 11
                default: mWarn << "Unhandled relocation type: " << rel.type; break;
            }

            dInfo2<kmod> << "Reloc " << rel.type << " at address " << (void *) addr << " with value " << value << " for symbol '"
                         << (rel.symbol ? rel.symbol->name : "unknown") << "' in section '" << (rel.in_section ? rel.in_section->name : "unknown") << "'"
                         << "(addend: " << rel.addend << ", offset: " << rel.offset << ")";
        }

        dInfo<kmod> << "Relocations applied successfully for module '" << dentry_name(file->dentry) << "'";
        return true;
    }

    bool ModuleELFInfo::EmitPLT() const
    {
        // allocate PLT
        const auto PLT = this->GetSectionByName(".plt");
        if (!PLT)
        {
            mWarn << "No section named '.plt' found in module '" << dentry_name(file->dentry) << "'";
            return false;
        }

        const auto nPages = ALIGN_UP_TO_PAGE(PLT->size) / MOS_PAGE_SIZE;
        PLT->load_address = VAlloc(nPages);
        dInfo<kmod> << "Loaded section '.plt' into memory at address " << (void *) PLT->load_address;

#if defined(__x86_64__)
        const u8 pltAsm[] = {
            0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, // jmp *<address>
            0x00, 0x00, 0x00, 0x00, 0x00,       // placeholder for address
            0x90, 0x90, 0x90, 0x90, 0x90,       // nop instructions (5 bytes)
            0xC3,                               // ret instruction
        };
#else
        mFatal << "PLT emission is not supported for this architecture";
        return false;
#endif

        for (size_t i = 0; i < pltEntries.size(); i++)
        {
            const auto &symbol = pltEntries[i];
            if (!symbol)
            {
                mWarn << "PLT entry " << i << " is null, skipping";
                continue;
            }

            const auto addr = symbol->ResolveRuntimeAddress();
            if (!addr)
            {
                mWarn << "PLT entry " << i << " has no runtime address, skipping";
                mWarn << "Symbol '" << symbol->name << "' is of type " << [=]()
                {
                    switch (symbol->type)
                    {
                        case Symbol::Undefined: return "Undefined";
                        case Symbol::Absolute: return "Absolute";
                        case Symbol::Regular: return "Regular";
                        default: return "Unknown";
                    }
                }();
                return false;
            }

            const auto offset = PLT->load_address + i * sizeof(pltAsm);
            if (offset + sizeof(pltAsm) > PLT->load_address + PLT->size)
            {
                mWarn << "PLT entry " << i << " exceeds section size, skipping";
                continue;
            }

            memcpy(reinterpret_cast<void *>(offset), pltAsm, sizeof(pltAsm));
            // Update the address in the PLT entry
            *reinterpret_cast<u64 *>(offset + 2) = *addr; // 2 bytes offset for the address in the PLT entry
        }

        dInfo<kmod> << "PLT entries emitted successfully for module '" << dentry_name(file->dentry) << "'";
        return true;
    }

    void ModuleELFInfo::DumpInfo() const
    {
        mInfo << "ModuleELFInfo for '" << dentry_name(file->dentry) << "':";
        mInfo << "  Sections: " << sections.size();
        for (const auto &[name, section] : sections)
        {
            mInfo << "    Section at index " << section->index << ", size " << (void *) section->size << ", load address " << (void *) section->load_address
                  << ", name: '" << section->name << "'";
        }
        mInfo << "  Symbols: " << symbols.size();
        for (const auto &symbol : symbols)
        {
            auto out = mInfo << "    Symbol '" << symbol->name << "' at index " << symbol->index << ", value " << symbol->value << ", size " << symbol->size;
            if (symbol->type == Symbol::Undefined)
            {
                out << ", type: Undefined, offset: " << symbol->undefined.offset;
            }
            else if (symbol->type == Symbol::Absolute)
            {
                out << ", type: Absolute, value: " << symbol->absolute.value;
            }
            else if (symbol->type == Symbol::Regular)
            {
                out << ", type: Regular, section: " << (symbol->regular.section ? symbol->regular.section->name : "null");
            }
        }

        mInfo << "  Relocations: " << relocations.size();
        for (const auto &reloc : relocations)
        {
            mInfo << "    Relocation in section '" << (reloc.in_section ? reloc.in_section->name : "null") << "' at offset " << reloc.offset << ", addend "
                  << reloc.addend << ", symbol: '" << (reloc.symbol ? reloc.symbol->name : "null") << "', type: " << reloc.type;
        }
        mInfo << "  String table size: " << strtab.size() << " bytes";
        mInfo << "  Section header string table size: " << shstrtab.size() << " bytes";
    }

    long ModuleELFInfo::FillData()
    {
        Elf64_Shdr sh_shstrtab;
        if (!file->pread(&sh_shstrtab, sizeof(Elf64_Shdr), header.sh_offset + header.sh_strtab_index * header.sh.entry_size))
        {
            mEmerg << "failed to read section header string table for '" << dentry_name(file->dentry) << "'";
            return -EIO;
        }

        shstrtab.resize(sh_shstrtab.sh_size);
        if (!file->pread(shstrtab.data(), shstrtab.capacity(), sh_shstrtab.sh_offset))
        {
            mEmerg << "failed to read section header string table content for '" << dentry_name(file->dentry) << "'";
            return -EIO;
        }

        for (u16 i = 0; i < header.sh.count; i++)
        {
            Elf64_Shdr sh_strtab;
            if (!file->pread(&sh_strtab, header.sh.entry_size, header.sh_offset + i * header.sh.entry_size))
            {
                mEmerg << "failed to read section header " << i << " for '" << dentry_name(file->dentry) << "'";
                return -EIO;
            }

            if (GetShStrTabEntry(sh_strtab.sh_name) == ".strtab" && sh_strtab.sh_type == static_cast<u32>(ElfSectionType::STRTAB))
            {
                dInfo<kmod> << "Found string table section '" << GetShStrTabEntry(sh_strtab.sh_name) << "' at offset " << sh_strtab.sh_offset << ", size "
                            << sh_strtab.sh_size;
                strtab.resize(sh_strtab.sh_size);
                if (!file->pread(strtab.data(), strtab.capacity(), sh_strtab.sh_offset))
                {
                    mEmerg << "failed to read string table content for '" << dentry_name(file->dentry) << "'";
                    return -EIO;
                }
                dInfo<kmod> << "String table for '" << dentry_name(file->dentry) << "' has " << (sh_strtab.sh_size / sizeof(char)) << " bytes";
                break;
            }
        }

        return 0;
    }

    std::optional<ptr_t> Symbol::ResolveRuntimeAddress()
    {
        if (type == Regular)
        {
            switch (symbol_type)
            {
                case NoType:
                {
                    return std::nullopt; // no runtime address for NoType symbols
                }
                case Object: // object symbols point to their section's load address
                case Function:
                {
                    if (regular.section)
                        return regular.section->load_address + value;
                    else
                        return std::nullopt; // function symbols point to their section's load address
                    break;
                }
                case SectionSymbol:
                { // section symbols point to the start of their section
                    if (regular.section)
                        return regular.section->load_address;
                    mWarn << "Section symbol '" << name << "' has no section associated";
                    return std::nullopt; // section symbols without a section are invalid
                }
                case File:
                { // file symbols do not have a runtime address, they are just metadata
                    mWarn << "File symbol '" << name << "' does not have a runtime address";
                    return std::nullopt;
                }
                default:
                {
                    mWarn << "Unknown symbol type: " << symbol_type << " for symbol: " << name;
                    return std::nullopt;
                }
            }
        }
        else if (type == Absolute)
        {
            return value; // absolute symbols are already addresses
        }
        else if (type == Undefined)
        {
            const auto ptr = kallsyms_get_symbol_address(name);
            if (!ptr)
            {
                mWarn << "GetRuntimeAddress called on undefined symbol: " << name;
                return std::nullopt;
            }

            dEmph<kmod> << "Resolved kernel symbol '" << name << "' to address " << (void *) ptr;
            value = ptr;
            type = Symbol::Absolute;
            return value;
        }

        mWarn << "GetRuntimeAddress called on non-regular or non-absolute symbol: " << name;
        return std::nullopt;
    }

    static PtrResult<mos::shared_ptr<ModuleELFInfo>> DoLoadKmodFromFile(FsBaseFile *file)
    {
        const auto mod = mos::make_shared<ModuleELFInfo>();
        mod->file = file;
        if (!elf_read_and_verify_executable(file, &mod->header, true))
        {
            mWarn << "Invalid ELF header in kernel module file '" << dentry_name(file->dentry) << "'";
            return -EINVAL;
        }

        // first find the strtab
        if (const auto ret = mod->FillData(); IS_ERR_VALUE(ret))
        {
            mEmerg << "failed to fill ELF data for '" << dentry_name(file->dentry) << "'";
            return ret;
        }

        dEmph<kmod> << "Kernel module file '" << dentry_name(file->dentry) << "' has " << mod->header.sh.count << " sections";

        const auto do_handle_regular_sections = [](ModuleELFInfo &mod, const Elf64_Shdr &sh, u16 id, mos::string_view sh_name) -> long
        {
            VMFlags flags = VMFlags();
            if (sh.sh_flags & SHF_WRITE)
                flags |= VM_WRITE;
            if (sh.sh_flags & SHF_ALLOC)
                flags |= VM_READ;
            if (sh.sh_flags & SHF_EXECINSTR)
                flags |= VM_EXEC;
            auto section = mos::make_shared<Section>();
            section->name = sh_name;
            section->index = id;
            section->file_offset = sh.sh_offset;
            section->load_address = 0;
            section->size = sh.sh_size;
            section->vmflags = flags;
            section->type = static_cast<ElfSectionType>(sh.sh_type);
            mod.sections[section->name] = section;
            return 0;
        };

        const auto do_handle_symbol_tables = [](ModuleELFInfo &mod, const Elf64_Shdr &sh, u16, mos::string_view sh_name) -> long
        {
            // Read the symbol table entries
            const auto num_symbols = sh.sh_size / sh.sh_entsize;
            for (u32 j = 0; j < num_symbols; j++)
            {
                Elf64_Sym sym;
                if (!mod.file->pread(&sym, sizeof(Elf64_Sym), sh.sh_offset + j * sizeof(Elf64_Sym)))
                {
                    mEmerg << "failed to read symbol entry " << j << " for section '" << sh_name << "'";
                    return -EIO;
                }

                const auto symbol = mos::make_shared<Symbol>();
                symbol->name = mod.GetStrTabEntry(sym.st_name);
                symbol->index = j;
                symbol->value = sym.st_value;
                symbol->size = sym.st_size;
                symbol->binding = static_cast<decltype(symbol->binding)>(ELF64_ST_BIND(sym.st_info));

                switch (const auto sym_type = ELF64_ST_TYPE(sym.st_info); sym_type)
                {
                    case STT_OBJECT: symbol->symbol_type = Symbol::Object; break;
                    case STT_FUNC: symbol->symbol_type = Symbol::Function; break;
                    case STT_NOTYPE: symbol->symbol_type = Symbol::NoType; break;
                    case STT_SECTION: symbol->symbol_type = Symbol::SectionSymbol; break;
                    case STT_FILE: symbol->symbol_type = Symbol::File; break;
                    default:
                    {
                        mWarn << "Unknown symbol type: " << sym_type;
                        symbol->symbol_type = Symbol::NoType;
                        break;
                    }
                }

                switch (sym.st_shndx)
                {
                    case SHN_UNDEF:
                    {
                        symbol->type = Symbol::Undefined;
                        symbol->undefined.offset = sym.st_value;
                        break;
                    }
                    case SHN_ABS:
                    {
                        symbol->type = Symbol::Absolute;
                        symbol->absolute.value = sym.st_value; // absolute value for absolute symbols
                        break;
                    }
                    case SHN_COMMON:
                    {
                        // common symbols are treated as undefined
                        symbol->type = Symbol::Undefined;
                        symbol->undefined.offset = 0; // offset in the section for common symbols
                        mWarn << "Symbol '" << symbol->name << "' is a common symbol, which is not supported in kernel modules";
                        break;
                    }
                    default:
                    {
                        auto section = mod.GetSectionByIndex(sym.st_shndx);
                        symbol->type = Symbol::Regular;
                        symbol->regular.section = section;
                        if (!symbol->regular.section)
                        {
                            mWarn << "Section with index " << sym.st_shndx << " not found for symbol '" << symbol->name << "'";
                            continue;
                        }

                        symbol->regular.section->symbols.push_back(symbol);
                        break;
                    }
                }

                mod.symbols.push_back(symbol);
            }

            return 0;
        };

        const auto do_handle_rela_sections = [](ModuleELFInfo &mod, const Elf64_Shdr &sh, u16, mos::string_view sh_name) -> long
        {
            if (sh_name.begins_with(".rela.debug_"))
                return 0;

            // Read the relocation entries
            const auto num_rela = sh.sh_size / sh.sh_entsize;
            for (u32 j = 0; j < num_rela; j++)
            {
                Elf64_Rela rela_entry;
                if (!mod.file->pread(&rela_entry, sizeof(Elf64_Rela), sh.sh_offset + j * sizeof(Elf64_Rela)))
                {
                    mEmerg << "failed to read relocation A entry " << j << " for section '" << sh_name << "'";
                    return -EIO;
                }

                // extract symbol index and type from r_info
                const auto sym_index = ELF64_R_SYM(rela_entry.r_info);
                const auto sym_type = ELF64_R_TYPE(rela_entry.r_info);

                const auto targetSection = mod.GetSectionByName(mos::string(sh_name.substr(5))); // remove ".rela" prefix
                if (!targetSection)
                {
                    mWarn << "Rela section '" << sh_name.substr(5) << "' not found in module sections!";
                    continue;
                }

                Relocation reloc;
                reloc.in_section = targetSection;
                reloc.symbol = mod.GetSymbolByIndex(sym_index);
                reloc.addend = rela_entry.r_addend;
                reloc.offset = rela_entry.r_offset;
                reloc.type = sym_type;

                if (sym_type == R_X86_64_PLT32)
                {
                    mod.pltEntries.push_back(reloc.symbol);
                    dInfo<kmod> << "Found PLT entry for symbol '" << reloc.symbol->name << "' in section '" << targetSection->name << "'";
                }

                mod.relocations.push_back(reloc);
            }
            return 0;
        };

        if (const auto ret = mod->ForEachSection(do_handle_regular_sections, { ElfSectionType::PROGBITS, ElfSectionType::NOBITS }); IS_ERR_VALUE(ret))
        {
            mEmerg << "failed to process regular sections for '" << dentry_name(file->dentry) << "'";
            return ret;
        }

        const auto do_handle_init_array_sections = [](ModuleELFInfo &mod, const Elf64_Shdr &sh, u16 id, mos::string_view sh_name) -> long
        {
            // Read the init array entries
            VMFlags flags = VMFlags();
            if (sh.sh_flags & SHF_WRITE)
                flags |= VM_WRITE;
            if (sh.sh_flags & SHF_ALLOC)
                flags |= VM_READ;
            if (sh.sh_flags & SHF_EXECINSTR)
                flags |= VM_EXEC;
            auto section = mos::make_shared<Section>();
            section->name = sh_name;
            section->index = id;
            section->file_offset = sh.sh_offset;
            section->load_address = 0;
            section->size = sh.sh_size;
            section->vmflags = flags;
            section->type = static_cast<ElfSectionType>(sh.sh_type);
            mod.sections[section->name] = section;
            return 0;
        };

        if (const auto ret = mod->ForEachSection(do_handle_init_array_sections, { ElfSectionType::INIT_ARRAY, ElfSectionType::FINI_ARRAY }); IS_ERR_VALUE(ret))
        {
            mEmerg << "failed to process INIT_ARRAY and FINI_ARRAY sections for '" << dentry_name(file->dentry) << "'";
            return ret;
        }

        if (const auto ret = mod->ForEachSection(do_handle_symbol_tables, { ElfSectionType::SYMTAB }); IS_ERR_VALUE(ret))
        {
            mEmerg << "failed to process symbol tables for '" << dentry_name(file->dentry) << "'";
            return ret;
        }

        if (const auto ret = mod->ForEachSection(do_handle_rela_sections, { ElfSectionType::RELA }); IS_ERR_VALUE(ret))
        {
            mEmerg << "failed to process RELA sections for '" << dentry_name(file->dentry) << "'";
            return ret;
        }

        if (!mod->pltEntries.empty())
        {
            // add a special PLT section for the module
            auto plt_section = mos::make_shared<Section>();
            plt_section->name = ".plt";
            plt_section->index = mod->sections.size();
            plt_section->file_offset = 0;
            plt_section->load_address = 0;
            plt_section->size = mod->pltEntries.size() * sizeof(Elf64_Rela);
            plt_section->vmflags = VMFlags(VM_READ | VM_EXEC);
            mod->sections[plt_section->name] = plt_section;
            dInfo<kmod> << "Added PLT section with " << mod->pltEntries.size() << " entries";
        }

        dInfo<kmod> << "Kernel module '" << dentry_name(file->dentry) << "' loaded with " << mod->sections.size() << " sections, " << mod->symbols.size()
                    << " symbols, and " << mod->relocations.size() << " relocations";
        return mod;
    }

    PtrResult<ptr<Module>> LoadModule(const mos::string &path)
    {
        if (path.empty())
        {
            mWarn << "Kernel module path cannot be empty";
            return -EINVAL;
        }

        if (!path_is_absolute(path))
        {
            mWarn << "Kernel module path must be absolute: " << path;
            return -EINVAL;
        }

        const auto name = mos::string(vfs_basename(path));

        if (const auto it = kmod_map.find(name); it != kmod_map.end())
        {
            mWarn << "Kernel module '" << name << "' is already loaded";
            return it.value();
        }

        auto file = vfs_openat(AT_FDCWD, path, OPEN_READ);
        if (file.isErr())
        {
            mWarn << "Failed to open kernel module file '" << path << "': " << file.getErr();
            return file.getErr();
        }

        const auto kmod = DoLoadKmodFromFile(file.get());
        if (kmod.isErr())
        {
            mWarn << "Failed to load kernel module '" << path << "': " << kmod.getErr();
            return kmod.getErr();
        }

        kmod->LoadIntoMemory();
        kmod->EmitPLT();
        kmod->PerformRelocation();
        kmod->LoadModuleBasicInfo();
        for (const auto &init_func : kmod->init_functions)
            init_func(); // call all init functions

        if (!kmod->entrypoint)
        {
            mWarn << "Kernel module '" << path << "' has no entrypoint defined";
            return -EINVAL;
        }

        const auto module = mos::make_shared<Module>(path, file->dentry->inode);
        module->module_info = kmod.get();
        kmod_map[kmod->GetModuleName()] = module;
        kmod->entrypoint(module);
        return module;
    }

    PtrResult<ptr<Module>> GetModule(const mos::string &name)
    {
        auto opt_module = kmod_map.get(name);
        if (!opt_module)
            return -ENOENT;
        return opt_module.value();
    }

    Module::Module(const mos::string &path, inode_t *)
    {
        dInfo<kmod> << "Loading kernel module: " << path;
    }

    void Module::ExportFunction(const mos::string &name, ExportedFunction handler)
    {
        if (!handler)
        {
            mWarn << "Cannot export null handler for function '" << name << "'";
            return;
        }

        if (exported_functions.contains(name))
        {
            mWarn << "Function '" << name << "' is already exported";
            return;
        }

        exported_functions[name] = handler;
        dInfo<kmod> << "Exported function '" << name << "' with handler: " << (void *) handler;
    }

    ValueResult<long> Module::TryCall(const mos::string &name, void *arg, size_t argSize)
    {
        if (!exported_functions.contains(name))
        {
            mWarn << "Module '" << this->name << "' does not export function '" << name << "'";
            return Err(-ENOENT);
        }

        auto &handler = exported_functions[name];
        if (!handler)
        {
            mWarn << "Module '" << this->name << "' has null handler for function '" << name << "'";
            return Err(-EINVAL);
        }

        return Ok(handler(arg, argSize));
    }
} // namespace mos::kmods
