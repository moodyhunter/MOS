#pragma once

#include "unit/unit.hpp"

#include <string>

struct Symlink : public Unit
{
    using Unit::Unit;

    std::string linkfile;
    std::string target;

  private:
    UnitType GetType() const override
    {
        return UnitType::Symlink;
    }
    bool Start() override;
    bool Stop() override;
    bool onLoad(const toml::table &data) override;
};
