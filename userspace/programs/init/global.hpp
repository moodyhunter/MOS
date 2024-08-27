#pragma once

#include "unit/unit.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <toml++/toml.hpp>

struct GlobalConfig
{
    std::string default_target;

    void parse(toml::table &data)
    {
        this->default_target = data["default_target"].value_or("default.target");
        data.erase("default_target");
    }
};

extern GlobalConfig global_config;
extern bool debug;
extern std::map<std::string, pid_t> service_pid;
extern std::map<std::string, std::shared_ptr<Unit>> units;

void load_configurations(const std::filesystem::path &config_path);
