#pragma once

#include "unit.hpp"

struct Path : public Unit
{
    using Unit::Unit;
    std::string path;

  private:
    bool do_start() override;
    bool do_stop() override;
    bool do_load(const toml::table &data) override;
    void do_print(std::ostream &os) const override;
};
