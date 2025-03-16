#pragma once

#include "unit.hpp"

#include <atomic>

struct Service : public Unit
{
    explicit Service(const std::string &id, const toml::table &table, std::shared_ptr<const Template> template_ = nullptr, const ArgumentMap &args = {});
    std::vector<std::string> exec;

    void OnExited(int status);

  private:
    UnitType GetType() const override
    {
        return UnitType::Service;
    }
    bool Start() override;
    bool Stop() override;
    void onPrint(std::ostream &os) const override;

  private:
    std::atomic<pid_t> main_pid = -1;
    int exit_status = -1;
};
