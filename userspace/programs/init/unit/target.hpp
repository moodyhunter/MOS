#pragma once

#include "unit.hpp"

struct Target : public Unit
{
    using Unit::Unit;

  private:
    bool do_start() override;
    bool do_stop() override;
    bool do_load(const toml::table &data) override;
};
