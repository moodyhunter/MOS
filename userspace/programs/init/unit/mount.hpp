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
    UnitType GetType() const override
    {
        return UnitType::Mount;
    }
    bool Start() override;
    bool Stop() override;
    bool onLoad(const toml::table &data) override;
    void onPrint(std::ostream &os) const override;
};
