// SPDX-License-Identifier: GPL-3.0-or-later

#include "input/input.hpp"
#include "render/renderer.hpp"
#include "windows/window-manager.hpp"
#include "windows/window.hpp"

#include <cstdlib>
#include <iostream>
#include <librpc/rpc.h>
#include <libsm.h>
#include <pb.h>

int main(int argc, char **argv)
{
    std::cout << "Display Daemon started with " << argc << " arguments." << std::endl;
    for (int i = 0; i < argc; ++i)
        std::cout << "argv[" << i << "] = " << argv[i] << std::endl;

    if (!DisplayManager::initialise_input())
    {
        std::cerr << "Failed to initialize input system." << std::endl;
        ReportServiceState(UnitStatus::Failed, "failed to initialize input system");
        return EXIT_FAILURE;
    }

    if (!DisplayManager::Render::Renderer->Initialize())
    {
        std::cerr << "Failed to initialize render system." << std::endl;
        ReportServiceState(UnitStatus::Failed, "failed to initialize render system");
        return EXIT_FAILURE;
    }

    ReportServiceState(UnitStatus::Started, "started");
    DisplayManager::Render::Renderer->RenderFullScreen();
    std::cout << "Display Daemon is running." << std::endl;

    DisplayManager::Render::Renderer->RenderFullScreen();
    DisplayManager::Windows::WindowManager->run();
}
