// SPDX-License-Identifier: GPL-3.0-or-later

#include "input/input.hpp"

#include "libsm.h"
#include "mos/syscall/usermode.h"
#include "render/renderer.hpp"
#include "windows/window-manager.hpp"

#include <librpc/rpc.h>
#include <mos/mos_global.h>
#include <stdio.h>
#include <thread>
#include <unistd.h>

constexpr auto INPUTD_MODULE_PATH = "/initrd/modules/inputd.ko";

using namespace DisplayManager;

static void process_keyboard_event(int scancode)
{
    // Process keyboard event
    printf("Keyboard event: scancode %d\n", scancode);
}

static void process_mouse_event(const u8 data[3])
{
    bool left_button = data[0] & 0x01;   // Left button pressed
    bool right_button = data[0] & 0x02;  // Right button pressed
    bool middle_button = data[0] & 0x04; // Middle button pressed

    const auto state = data[0];
    const auto x_movement = data[1] - ((state << 4) & 0x100);
    const auto y_movement = data[2] - ((state << 3) & 0x100);

    static Point cursor_position{ 0, 0 };

    // Update cursor position
    cursor_position.x += x_movement;
    cursor_position.y += -y_movement;
    const auto newPos = DisplayManager::Render::Renderer->SetCursorPosition(cursor_position);

    Input::MouseEvent mouseEvent{
        .leftButton = left_button,
        .rightButton = right_button,
        .middleButton = middle_button,
        .cursorPosition = newPos,
        .movement = { x_movement, -y_movement },
    };

    // Dispatch the mouse event to the window manager
    DisplayManager::Windows::WindowManager->DispatchMouseEvent(mouseEvent);
    cursor_position = newPos; // Update the global cursor position
}

static bool load_inputd_kmod()
{
    if (syscall_kmod_load(INPUTD_MODULE_PATH))
    {
        puts("Failed to load inputd kernel module");
        return false;
    }
    return true;
}

static void inputd_worker(fd_t fd)
{
    while (true)
    {
        int event_type;
        ssize_t bytes_read = read(fd, &event_type, sizeof(event_type));
        if (bytes_read < 0)
        {
            perror("Failed to read from inputd");
            ReportServiceState(UnitStatus::Failed, "inputd read failed");
            close(fd);
            return;
        }

        if (bytes_read == 0)
        {
            puts("No data available from inputd");
            continue; // No data, continue to the next iteration
        }

        if (event_type == 1) // Keyboard event
        {
            int scancode;
            bytes_read = read(fd, &scancode, sizeof(scancode));
            if (bytes_read < 0)
            {
                perror("Failed to read keyboard event");
                return;
            }
            process_keyboard_event(scancode);
        }
        else if (event_type == 2) // Mouse event
        {
            u8 mouse_data[3];
            bytes_read = read(fd, mouse_data, sizeof(mouse_data));
            if (bytes_read < 0 || bytes_read != 3)
            {
                perror("Failed to read mouse event");
                continue;
            }
            process_mouse_event(mouse_data);
        }
        else
        {
            puts("Unknown event type received");
        }
    }
}

bool DisplayManager::Input::InitializeInputd()
{
    load_inputd_kmod();
    fd_t fd = syscall_kmod_call("inputd", "subscribe", NULL, 0);
    if (fd < 0)
    {
        puts("Failed to subscribe to inputd");
        ReportServiceState(UnitStatus::Failed, "inputd subscription failed");
        return -1;
    }

    puts("Subscribed to inputd successfully");
    syscall_kmod_call("inputd", "enable", NULL, 0);
    std::thread worker_thread(inputd_worker, fd);
    worker_thread.detach();
    return true;
}
