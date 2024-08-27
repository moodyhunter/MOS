#include "mount.hpp"

#include <mos/syscall/usermode.h>

bool Mount::do_start()
{
    if (syscall_vfs_mount(device.c_str(), mount_point.c_str(), fs_type.c_str(), options.c_str()) != 0)
    {
        m_error = strerror(errno);
        return false;
    }

    return true;
}

bool Mount::do_stop()
{
    std::cout << "stopping mount " << id << std::endl;
    return true;
}

bool Mount::do_load(const toml::table &data)
{
    const auto mount = data["mount"].as_table();
    if (!mount)
    {
        std::cerr << "bad mount table" << std::endl;
        return false;
    }

    this->mount_point = (*mount)["mount_point"].value_or("unknown");
    this->fs_type = (*mount)["fs_type"].value_or("unknown");
    this->options = (*mount)["options"].value_or("unknown");
    this->device = (*mount)["device"].value_or("unknown");

    return true;
}

void Mount::do_print(std::ostream &os) const
{
    os << "  mount_point: " << this->mount_point << std::endl;
    os << "  fs_type: " << this->fs_type << std::endl;
    os << "  options: " << this->options << std::endl;
    os << "  device: " << this->device << std::endl;
}
