#include "service.hpp"

#include "ServiceManager.hpp"
#include "global.hpp"
#include "utils/ExecUtils.hpp"

RegisterUnit(service, Service);
RegisterUnit(driver, Service);

ServiceOptions::ServiceOptions(toml::node_view<toml::node> table_in)
{
    if (!table_in)
        return;

    if (!table_in.is_table())
    {
        std::cerr << "service: bad 'service' options" << std::endl;
        return;
    }

    auto &table = *table_in.as_table();
    const std::string state_change = table["state-change"].value_or("immediate");
    table.erase("state-change");

    {
        if (state_change == "immediate")
            stateChangeNotifyType = StateChangeNotifyType::Immediate;
        else if (state_change == "notify")
            stateChangeNotifyType = StateChangeNotifyType::Notify;
        else
            std::cerr << "service: bad state-change" << std::endl;
    }

    // warn if table["service"] contains unknown keys
    for (const auto &kv : table)
        std::cerr << "service: unknown key " << kv.first << std::endl;
}

Service::Service(const std::string &id, toml::table &table, std::shared_ptr<const Template> template_, const ArgumentMap &args)
    : Unit(id, table, template_, args), service_options(table["service"])
{
    table.erase("service");

    if (table["options"]["exec"].is_string())
        exec.push_back(PopArg(table, "exec"));
    else if (table["options"]["exec"].is_array())
        exec = GetArrayArg(table, "exec");
    else
        std::cerr << "service " << id << ": bad exec" << std::endl;
}

bool Service::Start()
{
    status.Starting("starting...");
    token = ExecUtils::GetRandomString();
    const auto pid = ExecUtils::DoFork(exec, token, GetBaseId());
    if (pid < 0)
    {
        std::cerr << "failed to start service " << id << std::endl;
        status.Failed("failed");
        return false;
    }

    main_pid = pid;
    if (service_options.stateChangeNotifyType == StateChangeNotifyType::Immediate)
    {
        status.Started("running");
        ServiceManager->OnUnitStarted(this);
    }

    return true;
}

bool Service::Stop()
{
    status.Stopping("stopping...");
    std::cout << "stopping service " << id << std::endl;
    const int pid = main_pid;
    if (pid == -1)
    {
        std::cerr << "service " << id << " not running" << std::endl;
        status.Inactive();
        return true;
    }

    kill(pid, SIGTERM);
    return true;
}

void Service::onPrint(std::ostream &os) const
{
    os << "  exec: ";
    for (const auto &e : this->exec)
        os << e << " ";
    os << std::endl;
    if (this->status.status == UnitStatus::UnitFailed)
        os << "failed: " << this->status.message << ", exit status: " << this->exit_status;
    os << std::endl;
}

void Service::OnExited(int status)
{
    status = status == W_EXITCODE(0, SIGTERM) ? 0 : status;
    this->exit_status = status;
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
    {
        this->status.Inactive();
    }
    else if (WIFEXITED(status))
    {
        std::cout << "service " << id << " exited with status " << WEXITSTATUS(status) << std::endl;
        this->status.Failed("exitcode: " + std::to_string(WEXITSTATUS(status)));
    }
    else if (WIFSIGNALED(status))
    {
        this->status.Failed("terminated by signal: " + std::to_string(WTERMSIG(status)));
    }
    else
    {
        this->status.Failed("unknown exit status: " + std::to_string(status));
    }
    ServiceManager->OnUnitStopped(this);
}

void Service::ChangeState(const UnitStatus &status)
{
    if (service_options.stateChangeNotifyType != StateChangeNotifyType::Notify)
    {
        std::cerr << "service " << id << " does not support state change notification" << std::endl;
        return;
    }

    const auto prev_status = this->status;
    this->status = status;

    // TODO: handle state change
    std::cerr << C_YELLOW << "service " << id << " state change: " << prev_status.status << " -> " << status.status << C_RESET << std::endl;

    if (status.status == UnitStatus::UnitStarted)
        ServiceManager->OnUnitStarted(this);
}
