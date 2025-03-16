#include "service.hpp"

#include "ServiceManager.hpp"
#include "logging.hpp"

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

RegisterUnit(service, Service);

Service::Service(const std::string &id, const toml::table &table, std::shared_ptr<const Template> template_, const ArgumentMap &args) : Unit(id, table, template_, args)
{
    const auto exec = table["exec"];
    if (!exec)
    {
        std::cerr << "service " << id << " missing exec" << std::endl;
        return;
    }

    if (exec.is_string())
        this->exec.push_back(exec.value_exact<std::string>().value());
    else if (exec.is_array())
        exec.visit(VectorAppender(this->exec));
    else
    {
        std::cerr << "service " << id << " bad exec" << std::endl;
    }

    for (auto &e : this->exec)
        e = ReplaceArgs(e);
}

bool Service::Start()
{
    status.Starting("starting...");
    const auto pid = fork();
    if (pid == 0)
    {
        const char **argv = (const char **) malloc((this->exec.size() + 1) * sizeof(char *));
        for (size_t i = 0; i < this->exec.size(); i++)
            argv[i] = this->exec[i].c_str();
        argv[this->exec.size()] = NULL;

        // redirect stdout and stderr to /tmp/log/service-id.log
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

        execv(this->exec[0].c_str(), (char **) argv);
        Debug << "unreachable code" << std::endl;
        return false;
    }
    else if (pid < 0)
    {
        std::cerr << "failed to start service " << id << std::endl;
        status.Failed("failed");
        return false;
    }

    status.Started("running");
    main_pid = pid;
    ServiceManager->OnUnitStarted(this, pid);
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
