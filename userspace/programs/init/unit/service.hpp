#pragma once

#include "unit.hpp"

struct Service : public Unit
{
    using Unit::Unit;
    std::vector<std::string> exec;

  private:
    UnitType GetType() const override
    {
        return UnitType::Service;
    }
    bool Start() override;
    bool Stop() override;
    bool onLoad(const toml::table &data) override;
    void onPrint(std::ostream &os) const override;

  private:
    int status = -1;
};
