#include "service.hpp"

#include "ServiceManager.hpp"
#include "logging.hpp"

#include <unistd.h>

bool Service::Start()
{
    SetStatus(UnitStatus::Starting);
    const auto pid = fork();
    if (pid == 0)
    {
        const char **argv = (const char **) malloc((this->exec.size() + 1) * sizeof(char *));
        for (size_t i = 0; i < this->exec.size(); i++)
            argv[i] = this->exec[i].c_str();
        argv[this->exec.size()] = NULL;

        execv(this->exec[0].c_str(), (char **) argv);

        Logger << "unreachable code" << std::endl;
        return false;
    }
    else if (pid < 0)
    {
        std::cerr << "failed to start service " << id << std::endl;
        return false;
    }

    SetStatus(UnitStatus::Running);
    ServiceManager->OnUnitStarted(this, pid);
    return true;
}

bool Service::Stop()
{
    SetStatus(UnitStatus::Stopping);
    std::cout << "stopping service " << id << std::endl;
    SetStatus(UnitStatus::Stopped);
    ServiceManager->OnUnitStopped(this);
    return true;
}

bool Service::onLoad(const toml::table &data)
{
    const auto exec = data["exec"];
    if (!exec)
    {
        std::cerr << "service " << id << " missing exec" << std::endl;
        return false;
    }

    if (exec.is_string())
    {
        this->exec.push_back(*exec.value_exact<std::string>());
    }
    else if (exec.is_array())
    {
        exec.as_array()->visit(VectorAppender(this->exec));
    }
    else
    {
        std::cerr << "service " << id << " bad exec" << std::endl;
        return false;
    }

    return true;
}

void Service::onPrint(std::ostream &os) const
{
    os << "  exec: ";
    for (const auto &e : this->exec)
        os << e << " ";
    os << std::endl;
}
