#pragma once

#include "unit/unit.hpp"

#include <string>

struct Symlink : public Unit
{
    explicit Symlink(const std::string &id, toml::table &table, std::shared_ptr<const Template> template_ = nullptr, const ArgumentMap &args = {});

    const std::string linkfile;
    const std::string target;

  private:
    UnitType GetType() const override
    {
        return UnitType::Symlink;
    }
    bool Start() override;
    bool Stop() override;
};
