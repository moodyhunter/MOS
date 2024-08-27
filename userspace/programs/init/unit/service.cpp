#include "service.hpp"

#include "global.hpp"

#include <unistd.h>

bool Service::do_start()
{
    this->pid = fork();
    if (this->pid == 0)
    {
        const char **argv = (const char **) malloc(sizeof(char *));
        int argc = 1;
        argv[0] = this->exec[0].c_str();

        for (size_t i = 1; i < this->exec.size(); i++)
        {
            argc++;
            argv = (const char **) realloc(argv, argc * sizeof(char *));
            argv[argc - 1] = this->exec[i].c_str();
        }

        argv = (const char **) realloc(argv, (argc + 1) * sizeof(char *));
        argv[argc] = NULL;

        execv(this->exec[0].c_str(), (char **) argv);
        return false;
    }
    else if (this->pid < 0)
    {
        std::cerr << "failed to start service " << id << std::endl;
        return false;
    }

    service_pid[id] = this->pid;
    return true;
}

bool Service::do_stop()
{
    std::cout << "stopping service " << id << std::endl;
    return true;
}

bool Service::do_load(const toml::table &data)
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
        exec.as_array()->visit(array_append_visitor(this->exec));
    }
    else
    {
        std::cerr << "service " << id << " bad exec" << std::endl;
        return false;
    }

    return true;
}

void Service::do_print(std::ostream &os) const
{
    os << "  exec: ";
    for (const auto &e : this->exec)
        os << e << " ";
    os << std::endl;
}
