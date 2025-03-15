#include "global.hpp"

#include <filesystem>
#include <glob.h>
#include <iostream>
#include <ranges>

static std::vector<std::string> ExpandGlob(const std::string &pattern)
{
    glob_t globobj;
    glob(pattern.c_str(), 0, nullptr, &globobj);

    std::vector<std::string> paths;
    for (size_t i = 0; i < globobj.gl_pathc; ++i)
        paths.push_back(globobj.gl_pathv[i]);

    globfree(&globobj);
    return paths;
}

static std::vector<std::string> ExpandIncludePaths(const toml::node_view<toml::node> &node)
{
    std::vector<std::string> includes;
    if (node.is_array())
    {
        if (!node.is_homogeneous())
            std::cerr << "non-string elements in include array, they will be ignored" << std::endl;

        includes = *node.as_array() |                                                     //
                   std::views::filter([](const auto &v) { return v.is_string(); }) |      //
                   std::views::transform([](const auto &v) { return **v.as_string(); }) | //
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

    return includes | std::views::transform(ExpandGlob) | std::views::join | std::ranges::to<std::vector>();
}

std::vector<toml::table> ReadAllConfig(const std::filesystem::path &config_path)
{
    std::vector<toml::table> tables;

    auto mainTable = toml::parse_file(config_path.string());

    const auto old_path = std::filesystem::current_path();
    std::filesystem::current_path(config_path.parent_path());

    const auto includes = ExpandIncludePaths(mainTable["include"]);
    mainTable.erase("include");

    tables.push_back(std::move(mainTable));
    for (const auto &inc : includes)
        tables.push_back(toml::parse_file(inc));
    std::filesystem::current_path(old_path);

    return tables;
}
