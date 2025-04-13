#include "service.hpp"

#include "ServiceManager.hpp"
#include "logging.hpp"

#include <bits/posix/posix_stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

RegisterUnit(service, Service);

static std::string GetRandomString(size_t length = 32)
{
    static const char alphanum[] = "0123456789"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "abcdefghijklmnopqrstuvwxyz";
    std::string s;
    s.reserve(length);
    for (size_t i = 0; i < length; i++)
        s.push_back(alphanum[rand() % (sizeof(alphanum) - 1)]);
    return s;
}

static void RedirectLogFd(const std::string &id)
{
    const std::string log_path = "/tmp/log/" + id + ".log";
    const int log_fd = open(log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log_fd == -1)
    {
        std::cerr << "failed to open log file " << log_path << std::endl;
        exit(1);
    }
    dup2(log_fd, STDOUT_FILENO);
    dup2(log_fd, STDERR_FILENO);
    close(log_fd);
}

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
        exec.push_back(GetArg(table, "exec"));
    else if (table["options"]["exec"].is_array())
        exec = GetArrayArg(table, "exec");
    else
        std::cerr << "service " << id << ": bad exec" << std::endl;
}

bool Service::Start()
{
    status.Starting("starting...");
    token = GetRandomString();
    const auto pid = fork();
    if (pid == 0)
    {
        std::vector<const char *> args;
        for (const auto &arg : exec)
            args.push_back(arg.c_str());
        args.push_back(nullptr);

        // redirect stdout and stderr to /tmp/log/service-id.log
        RedirectLogFd(id);
        setenv("MOS_SERVICE_TOKEN", token.c_str(), true);

        execve(this->exec[0].c_str(), (char **) args.data(), environ);
        Debug << "unreachable code" << std::endl;
        __builtin_unreachable();
        return false;
    }
    else if (pid < 0)
    {
        std::cerr << "failed to start service " << id << std::endl;
        status.Failed("failed");
        return false;
    }

    main_pid = pid;
    if (service_options.stateChangeNotifyType == StateChangeNotifyType::Immediate)
    {
        status.Started("running");
        ServiceManager->OnUnitStarted(this, pid);
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
        std::cout << "service " << id << " exited normally" << std::endl;
        this->status.Inactive();
    }
    else if (WIFEXITED(status))
    {
        std::cout << "service " << id << " exited with status " << WEXITSTATUS(status) << std::endl;
        this->status.Failed("exitcode: " + std::to_string(WEXITSTATUS(status)));
    }
    else if (WIFSIGNALED(status))
    {
        std::cout << "service " << id << " terminated by signal " << WTERMSIG(status) << std::endl;
        this->status.Failed("terminated by signal: " + std::to_string(WTERMSIG(status)));
    }
    else
    {
        std::cout << "service " << id << " unknown exit status: " << status << std::endl;
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
    std::cerr << "service " << id << " state change: " << prev_status.status << " -> " << status.status << std::endl;
}
