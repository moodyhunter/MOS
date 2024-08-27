#include "global.hpp"
#include "unit/mount.hpp"
#include "unit/path.hpp"
#include "unit/service.hpp"
#include "unit/symlink.hpp"
#include "unit/target.hpp"
#include "unit/unit.hpp"

#include <filesystem>
#include <glob.h>
#include <iostream>
#include <ranges>

using namespace std::string_literals;

static std::vector<std::string> expand_glob(const std::string &pattern)
{
    glob_t glob_result;
    glob(pattern.c_str(), 0, nullptr, &glob_result);

    std::vector<std::string> paths;
    for (size_t i = 0; i < glob_result.gl_pathc; ++i)
        paths.push_back(glob_result.gl_pathv[i]);
    globfree(&glob_result);
    return paths;
}

static std::vector<std::string> expand_includes(const toml::node_view<toml::node> &node)
{
    std::vector<std::string> includes;
    if (node.is_array())
    {
        if (!node.is_homogeneous())
            std::cerr << "non-string elements in include array, they will be ignored" << std::endl;

        includes = *node.as_array() |                                                                //
                   std::views::filter([](const toml::node &v) { return v.is_string(); }) |           //
                   std::views::transform([](const toml::node &v) { return v.as_string()->get(); }) | //
                   std::ranges::to<std::vector>();
    }
    else if (node.is_string())
    {
        includes.push_back(node.as_string()->get());
    }
    else
    {
        std::cerr << "bad include paths, expect string or array but got " << node.type() << std::endl;
    }

    return includes | std::views::transform(expand_glob) | std::views::join | std::ranges::to<std::vector>();
}

static std::unique_ptr<Unit> create_unit(const std::string &id, const toml::table *data)
{
    if (!data || !data->is_table() || data->empty() || !data->contains("type"))
    {
        std::cerr << "bad unit, expect table with type" << std::endl;
        return nullptr;
    }

    const auto type = (*data)["type"];
    if (!type.is_string())
    {
        std::cerr << "bad type, expect string" << std::endl;
        return nullptr;
    }

    const auto type_string = type.as_string()->get();

    std::unique_ptr<Unit> unit;
    if (type_string == "service")
        unit = std::make_unique<Service>(id);
    else if (type_string == "target")
        unit = std::make_unique<Target>(id);
    else if (type_string == "path")
        unit = std::make_unique<Path>(id);
    else if (type_string == "mount")
        unit = std::make_unique<Mount>(id);
    else if (type_string == "symlink")
        unit = std::make_unique<Symlink>(id);
    else
        std::cerr << "unknown type " << type_string << std::endl;

    if (!unit)
    {
        std::cerr << "failed to create unit" << std::endl;
        return nullptr;
    }

    unit->load(*data);
    return unit;
}

static std::map<std::string, std::shared_ptr<Unit>> parse_units(const std::map<std::string, toml::table> &files)
{
    std::map<std::string, std::shared_ptr<Unit>> units;
    for (const auto &[path, table] : files)
    {
        for (const auto &[key, value] : table)
        {
            const auto table = value.as_table();
            if (!table)
            {
                std::cerr << "bad table" << std::endl;
                continue;
            }

            for (const auto &[subkey, real_table] : *table)
            {
                const auto unit_id = key.data() + "."s + subkey.data();
                if (units.contains(unit_id))
                {
                    std::cerr << "unit " << unit_id << " already exists" << std::endl;
                    continue;
                }

                auto unit = create_unit(unit_id, real_table.as_table());
                if (unit)
                    units[unit_id] = std::move(unit);
            }
        }
    }

    return units;
}

std::map<std::string, toml::table> read_all_config(const std::filesystem::path &config_path)
{
    toml::table main_table = toml::parse_file(config_path.string());
    std::map<std::string, toml::table> unit_configurations;

    {
        const auto old_path = std::filesystem::current_path();
        std::filesystem::current_path(config_path.parent_path());

        for (const auto &include_path : expand_includes(main_table["include"]))
            unit_configurations[include_path] = toml::parse_file(include_path);

        main_table.erase("include");

        std::filesystem::current_path(old_path);
    }

    global_config.parse(main_table); // parse global config, will remove it from main_table

    unit_configurations[config_path.string()] = main_table;
    return unit_configurations;
}

void load_configurations(const std::filesystem::path &config_path)
{
    units = parse_units(read_all_config(config_path));

    // now resolve dependencies
    for (const auto &[id, unit] : units)
    {
        for (const auto &dep_id : unit->depends_on)
        {
            if (!units.contains(dep_id))
            {
                std::cerr << "unit " << id << " depends on non-existent unit " << dep_id << std::endl;
                continue;
            }
        }

        for (const auto &part_id : unit->part_of)
        {
            // add unit to part_of unit's depends_on
            if (!units.contains(part_id))
            {
                std::cerr << "unit " << id << " is part of non-existent unit " << part_id << std::endl;
                continue;
            }

            units[part_id]->depends_on.push_back(id);
        }
    }
}
