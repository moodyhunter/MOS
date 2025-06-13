// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// The InheritedUnit class wraps another Unit and delegates all operations to it.
#include "units/unit.hpp"

class InheritedUnit : public IUnit
{
  public:
    explicit InheritedUnit(const std::string &id, std::shared_ptr<IUnit> childUnit);

    UnitType GetType() const override;
    bool Start() override;
    bool Stop() override;
    const UnitStatus &GetStatus() const override;
    std::vector<std::string> GetDependencies() const override;
    std::vector<std::string> GetPartOf() const override;
    std::optional<std::string> GetFailReason() const override;
    std::string GetChildId() const;
    std::string GetDescription() const override;
    void AddDependency(const std::string &depName) override;

  private:
    const std::shared_ptr<IUnit> childUnit_;
};
