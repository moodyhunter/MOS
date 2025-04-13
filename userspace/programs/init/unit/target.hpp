#pragma once

#include "unit.hpp"

struct Target : public Unit
{
    explicit Target(const std::string &id, toml::table &table, std::shared_ptr<const Template> template_ = nullptr, const ArgumentMap &args = {});

  private:
    UnitType GetType() const override
    {
        return UnitType::Target;
    }
    bool Start() override;
    bool Stop() override;
};
