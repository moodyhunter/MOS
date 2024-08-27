#pragma once

#include "unit.hpp"

struct Service : public Unit
{
    using Unit::Unit;
    std::vector<std::string> exec;

  private:
    bool do_start() override;
    bool do_stop() override;
    bool do_load(const toml::table &data) override;
    void do_print(std::ostream &os) const override;

  private:
    pid_t pid = -1;
    int status = -1;
};
