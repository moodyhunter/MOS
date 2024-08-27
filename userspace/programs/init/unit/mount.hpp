#pragma once

#include "unit.hpp"

struct Mount : public Unit
{
    using Unit::Unit;
    std::string mount_point;
    std::string fs_type;
    std::string options;
    std::string device;

  private:
    bool do_start() override;
    bool do_stop() override;
    bool do_load(const toml::table &data) override;
    void do_print(std::ostream &os) const override;
};
