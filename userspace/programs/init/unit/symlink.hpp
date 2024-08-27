#pragma once

#include "unit/unit.hpp"

#include <string>

struct Symlink : public Unit
{
    using Unit::Unit;

    std::string linkfile;
    std::string target;

  private:
    bool do_start() override;
    bool do_stop() override;
    bool do_load(const toml::table &data) override;
};
