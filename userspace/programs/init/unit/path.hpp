#pragma once

#include "unit.hpp"

struct Path : public Unit
{
    using Unit::Unit;
    std::string path;

  private:
    UnitType GetType() const override
    {
        return UnitType::Path;
    }
    bool Start() override;
    bool Stop() override;
    bool onLoad(const toml::table &data) override;
    void onPrint(std::ostream &os) const override;
};
