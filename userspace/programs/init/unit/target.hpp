#pragma once

#include "unit.hpp"

struct Target : public Unit
{
    using Unit::Unit;

  private:
    UnitType GetType() const override
    {
        return UnitType::Target;
    }
    bool Start() override;
    bool Stop() override;
    bool onLoad(const toml::table &data) override;
};
