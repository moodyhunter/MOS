// SPDX-License-Identifier: GPL-3.0-or-later

#include "input/input.hpp"
#include "mos/syscall/usermode.h"
#include "render/renderer.hpp"
#include "windows/window-manager.hpp"

#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <librpc/rpc.h>
#include <libsm.h>
#include <mos/mm/mm_types.h>
#include <pb.h>

void *renderBuffer = NULL;

int main(int, char **)
{
    if (!DisplayManager::Render::Renderer->Initialize())
    {
        std::cerr << "Failed to initialize render system." << std::endl;
        ReportServiceState(UnitStatus::Failed, "failed to initialize render system");
        return EXIT_FAILURE;
    }

    if (!DisplayManager::Input::InitializeInputd())
    {
        std::cerr << "Failed to initialize input system." << std::endl;
        ReportServiceState(UnitStatus::Failed, "failed to initialize input system");
        return EXIT_FAILURE;
    }

    const auto fd = open("/tmp/gpu.virtio.memfd", O_RDWR);
    if (fd < 0)
    {
        std::cerr << "Failed to create memfd for GPU." << std::endl;
        ReportServiceState(UnitStatus::Failed, "failed to create memfd for GPU");
        return EXIT_FAILURE;
    }

    const auto size = DisplayManager::Render::Renderer->GetDisplaySize();

    renderBuffer = syscall_mmap_file(0, size.width * size.height * sizeof(u32), mem_perm_t(MEM_PERM_READ | MEM_PERM_WRITE), MMAP_SHARED, fd, 0);

    DisplayManager::Render::Renderer->RenderFullScreen();
    ReportServiceState(UnitStatus::Started, "started");
    DisplayManager::Windows::WindowManager->run();
    return EXIT_SUCCESS;
}
