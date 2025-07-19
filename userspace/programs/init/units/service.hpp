#pragma once

#include "unit.hpp"

#include <atomic>

enum class StateChangeNotifyType
{
    Immediate, ///< Service state change is applied immediately
    Notify,    ///< Service executable is capable of telling us that it has started
};

struct ServiceOptions
{
    explicit ServiceOptions(toml::node_view<toml::node> table);

    StateChangeNotifyType stateChangeNotifyType = StateChangeNotifyType::Immediate;
    bool redirect = true; ///< Redirect stdout/stderr to syslog daemon
};

struct Service : public Unit
{
    explicit Service(const std::string &id, toml::table &table, std::shared_ptr<const Template> template_ = nullptr, const ArgumentMap &args = {});
    std::vector<std::string> exec;

    void OnExited(int status);

    std::string GetToken() const
    {
        return token;
    }

    pid_t GetMainPid() const
    {
        return main_pid.load();
    }

    void ChangeState(const UnitStatus &status);

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
    std::string token;
    ServiceOptions service_options;
};
