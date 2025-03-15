#pragma once

#include <filesystem>
#include <toml++/toml.hpp>

using namespace std::string_literals;

#define RED(text)   "\033[1;31m" text "\033[0m"
#define GREEN(text) "\033[1;32m" text "\033[0m"

#define FAILED()   RED("[FAILED]")
#define OK()       GREEN("[  OK  ]")
#define STARTING() "\033[0m         "

std::vector<toml::table> ReadAllConfig(const std::filesystem::path &config_path);
