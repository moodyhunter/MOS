#include "mount.hpp"

#include "ServiceManager.hpp"

#include <mos/syscall/usermode.h>

RegisterUnit(mount, Mount);

Mount::Mount(const std::string &id, toml::table &table, std::shared_ptr<const Template> template_, const ArgumentMap &args)
    : Unit(id, table, template_, args),          //
      mount_point(GetArg(table, "mount_point")), //
      fs_type(GetArg(table, "fs_type")),         //
      options(GetArg(table, "options")),         //
      device(GetArg(table, "device"))            //
{
    if (mount_point.empty())
        std::cerr << "mount: missing mount_point" << std::endl;
    if (fs_type.empty())
        std::cerr << "mount: missing fs_type" << std::endl;
    if (device.empty())
        std::cerr << "mount: missing device" << std::endl;
}

bool Mount::Start()
{
    status.Starting();
    if (syscall_vfs_mount(device.c_str(), mount_point.c_str(), fs_type.c_str(), options.c_str()) != 0)
    {
        status.Failed(strerror(errno));
        return false;
    }

    status.Started();
    ServiceManager->OnUnitStarted(this);
    return true;
}

bool Mount::Stop()
{
    status.Stopping();
    std::cout << "stopping mount " << id << std::endl;
    status.Inactive();
    const auto err = syscall_vfs_unmount(mount_point.c_str());
    if (err != 0)
    {
        errno = -err;
        status.Failed(strerror(errno));
        return false;
    }
    ServiceManager->OnUnitStopped(this);
    return true;
}

void Mount::onPrint(std::ostream &os) const
{
    os << "  mount_point: " << this->mount_point << std::endl;
    os << "  fs_type: " << this->fs_type << std::endl;
    os << "  options: " << this->options << std::endl;
    os << "  device: " << this->device << std::endl;
}
