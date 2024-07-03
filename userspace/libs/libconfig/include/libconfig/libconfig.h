// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <ranges>
#include <string>
#include <vector>

class Config
{
  public:
    static inline constexpr std::string DEFAULT_SECTION = "global";

    typedef std::string KeyType;
    typedef std::string ValueType;
    typedef std::pair<KeyType, ValueType> EntryType;
    typedef std::vector<EntryType> SectionContentType;
    typedef std::pair<std::string, SectionContentType> SectionType;

  public:
    static std::optional<Config> from_file(const std::string &file_path);

    Config() = default;
    ~Config() = default;

    template<typename Pred>
    requires std::predicate<Pred, SectionType> std::vector<Config::SectionType> get_sections(Pred pred = [](auto) { return true; }) const
    {
        return sections | std::ranges::views::filter(pred) | std::ranges::to<std::vector<SectionType>>();
    }

    std::optional<SectionType> get_section(const std::string &section_name) const;

    std::vector<EntryType> get_entry(const std::string &section_name, const std::string &key) const;

    std::vector<EntryType> get_entries(const std::string &section_name) const;

  public:
    std::vector<SectionType> sections;
};
