// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "unit/unit.hpp"

struct Device : public Unit
{
    explicit Device(const std::string &id, toml::table &table, std::shared_ptr<const Template> template_ = nullptr, const ArgumentMap &args = {});

    const std::string driver;
    const std::vector<std::string> driver_args;

  private:
    UnitType GetType() const override
    {
        return UnitType::Device;
    }
    bool Start() override;
    bool Stop() override;
    void onPrint(std::ostream &os) const override;
};
