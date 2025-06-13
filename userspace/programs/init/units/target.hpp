#pragma once

#include "unit.hpp"

struct Target : public Unit
{
    explicit Target(const std::string &id, toml::table &table, std::shared_ptr<const Template> template_ = nullptr, const ArgumentMap &args = {});

    std::vector<std::string> GetMembers() const
    {
        return members;
    }

    void AddMember(const std::string &unitId)
    {
        members.push_back(unitId);
    }

  private:
    UnitType GetType() const override
    {
        return UnitType::Target;
    }
    bool Start() override;
    bool Stop() override;

  private:
    std::vector<std::string> members; // units that are part of this target
};
