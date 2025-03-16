#pragma once

#include "unit.hpp"

struct Mount : public Unit
{
    explicit Mount(const std::string &id, const toml::table &table, std::shared_ptr<const Template> template_ = nullptr, const ArgumentMap &args = {});

    const std::string mount_point;
    const std::string fs_type;
    const std::string options;
    const std::string device;

  private:
    UnitType GetType() const override
    {
        return UnitType::Mount;
    }
    bool Start() override;
    bool Stop() override;
    void onPrint(std::ostream &os) const override;
};
