// SPDX-License-Identifier: GPL-3.0-or-later

#include "windows/window.hpp"

#include "render/renderer.hpp"
#include "utils/SubView.hpp"
#include "windows/window-manager.hpp"

#include <iostream>
#include <stdexcept>

using namespace DisplayManager;
using namespace DisplayManager::Windows;

Window::Window(u64 wId, const std::string &title, Point pos, Size size, WindowType windowType)
    : windowId(wId), windowType(windowType), position(pos), size(size), title(title)
{
    // Allocate backing buffer for the window content
    const auto buffersize = size.width * size.height * sizeof(uint32_t);
    backingBuffer = static_cast<pb_bytes_array_t *>(malloc(PB_BYTES_ARRAY_T_ALLOCSIZE(buffersize)));
    if (!backingBuffer)
    {
        throw std::runtime_error("Failed to allocate backing buffer for window");
    }
    backingBuffer->size = buffersize;
    memset(backingBuffer->bytes, 0xff, buffersize); // Initialize the buffer to white color
}

Window::~Window()
{
    if (backingBuffer)
    {
        free(backingBuffer);
        backingBuffer = nullptr;
    }
}

bool Window::UpdateContent(const Region &local, const void *data, size_t datalen)
{
    if (!backingBuffer)
        throw std::runtime_error("Backing buffer was not allocated");

    if (local.origin.x < 0 || local.origin.y < 0 || local.origin.x + local.size.width > size.width || local.origin.y + local.size.height > size.height)
    {
        std::cerr << "Region is out of bounds for window size." << std::endl;
        return false;
    }

    if (datalen < local.size.width * local.size.height * sizeof(uint32_t))
    {
        std::cerr << "Content buffer is smaller than expected." << std::endl;
        return false;
    }

    Utils::SubView<uint32_t> subView(backingBuffer->bytes, backingBuffer->size, size, local);
    for (int y = 0; y < local.size.height; ++y)
    {
        for (int x = 0; x < local.size.width; ++x)
        {
            size_t index = (y * local.size.width + x);
            subView[{ x, y }] = reinterpret_cast<const uint32_t *>(data)[index];
        }
    }

    return true;
}

bool Window::GetRegionContent(const Region &local, Utils::SubView<uint32_t> &destination)
{
    if (!backingBuffer)
        throw std::runtime_error("Backing buffer was not allocated");

    const Utils::SubView<uint32_t> subView(backingBuffer->bytes, backingBuffer->size, this->size, local);
    for (int y = 0; y < local.size.height; ++y)
    {
        for (int x = 0; x < local.size.width; ++x)
        {
            const auto currentArgb = destination[{ x, y }];
            const auto renderArgb = subView[{ x, y }];
            destination[{ x, y }] = Render::AlphaBlend(currentArgb, renderArgb); // blend with the existing pixel
        }
    }
    return true;
}

bool Window::HandleMouseEvent(const Input::MouseEvent &event)
{
    static bool isLeftButtonPressed = false;
    if (event.leftButton)
    {
        if (!isLeftButtonPressed)
        {
            isLeftButtonPressed = true;
            std::cout << "Left button pressed on window: " << title << std::endl;
            WindowManager->BringWindowToFront(windowId); // Bring this window to the front
        }

        if (isLeftButtonPressed)
        {
            if (event.movement)
            {
                // Move the window to the new position based on mouse movement
                Point newPosition = position;
                newPosition.x += event.movement.x;
                newPosition.y += event.movement.y;
                WindowManager->MoveWindowTo(windowId, newPosition);
                position = newPosition;
            }
        }
    }
    else
    {
        if (isLeftButtonPressed)
        {
            isLeftButtonPressed = false;
            std::cout << "Left button released on window: " << title << std::endl;
        }
    }

    return true; // Indicate that the event was handled
}
