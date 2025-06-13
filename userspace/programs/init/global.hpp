#pragma once

#include <filesystem>
#include <toml++/toml.hpp>

using namespace std::string_literals;

#define RED(text)     "\033[1;31m" text "\033[0m"
#define GREEN(text)   "\033[1;32m" text "\033[0m"
#define YELLOW(text)  "\033[1;33m" text "\033[0m"
#define BLUE(text)    "\033[1;34m" text "\033[0m"
#define MAGENTA(text) "\033[1;35m" text "\033[0m"
#define CYAN(text)    "\033[1;36m" text "\033[0m"
#define WHITE(text)   "\033[1;37m" text "\033[0m"
#define RESET(text)   "\033[0m" text "\033[0m"

#define C_RED    "\033[1;31m"
#define C_GREEN  "\033[1;32m"
#define C_YELLOW "\033[1;33m"
#define C_BLUE   "\033[1;34m"
#define C_GRAY   "\033[1;30m"
#define C_WHITE  "\033[1;37m"
#define C_RESET  "\033[0m"

#define FAILED()   RED("[FAILED]")
#define OK()       GREEN("[  OK  ]")
#define STARTING() "\033[0m         "
#define STOPPING() "\033[0m         "

std::vector<toml::table> ReadAllConfig(const std::filesystem::path &config_path);
