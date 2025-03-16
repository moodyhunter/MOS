#pragma once

#include "unit.hpp"

struct Path : public Unit
{
    explicit Path(const std::string &id, const toml::table &table, std::shared_ptr<const Template> template_ = nullptr, const ArgumentMap &args = {});
    const std::string path;

  private:
    UnitType GetType() const override
    {
        return UnitType::Path;
    }
    bool Start() override;
    bool Stop() override;
    void onPrint(std::ostream &os) const override;
};
