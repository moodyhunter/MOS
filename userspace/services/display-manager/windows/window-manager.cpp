// SPDX-License-Identifier: GPL-3.0-or-later

#include "windows/window-manager.hpp"

#include "proto/graphics-dm.service.h"
#include "render/renderer.hpp"
#include "utils/common.hpp"

#include <iostream>
#include <librpc/rpc.h>
#include <vector>

using namespace DisplayManager;
using namespace DisplayManager::Windows;

struct RpcClientContext
{
    std::list<u64> windowIds; ///< List of window IDs associated with this client
};

WindowManagerClass::WindowManagerClass() : IWindowManagerService(WINDOW_MANAGER_SERVICE_NAME)
{
}

WindowManagerClass::~WindowManagerClass()
{
}

std::shared_ptr<Window> WindowManagerClass::CreateWindow(const std::string &name, Point pos, Size size, WindowType type)
{
    const auto windowId = nextWindowId++;
    std::cerr << "Creating window with ID: " << windowId << ", Title: " << name << ", Size: " << size << std::endl;

    const auto window = std::make_shared<Window>(windowId, name, pos, size, type);
    windows[window->windowId] = window;

    if (type == WindowType::Cursor)
    {
        TopMostWindows.push_front(window->windowId); // Add to the front of topmost windows
    }
    else
    {
        ZOrder.push_front(window->windowId); // Add to the front of Z-order (topmost)
    }
    return window;
}

Delta WindowManagerClass::MoveWindowTo(u64 wId, Point newPosition)
{
    static std::mutex mutex;
    std::lock_guard lock{ mutex };

    auto window = GetWindow(wId);
    if (!window)
    {
        std::cerr << "Window with ID " << wId << " not found." << std::endl;
        return { 0, 0 };
    }

    const auto oldRegion = window->GetWindowRegion();
    window->position = newPosition; // Update the position of the window
    const auto newRegion = window->GetWindowRegion();

    const auto totalDamageRegion = oldRegion.GetUnion(newRegion);
    Render::Renderer->DamageGlobal(totalDamageRegion);

    return { newRegion.origin.x - oldRegion.origin.x, newRegion.origin.y - oldRegion.origin.y };
}

void WindowManagerClass::DispatchMouseEvent(const Input::MouseEvent &event)
{
    // iterate through all windows (according to topmost and Z-order)
    std::vector<u64> allWindows;
    allWindows.insert(allWindows.end(), TopMostWindows.begin(), TopMostWindows.end());
    allWindows.insert(allWindows.end(), ZOrder.begin(), ZOrder.end());

    for (const auto &wId : allWindows)
    {
        auto window = GetWindow(wId);
        if (!window)
            continue; // Skip if window not found

        if (window->windowType == WindowType::Cursor || window->windowType == WindowType::Background)
            continue;

        // Check if the mouse event is within the window's region
        const auto region = window->GetWindowRegion();
        if (region.InRegion(event.cursorPosition))
        {
            if (window->HandleMouseEvent(event))
                return;

            // If the window did not handle the event, continue to the next window
        }
    }
}

void DisplayManager::Windows::WindowManagerClass::BringWindowToFront(u64 wId)
{
    auto it = std::find(ZOrder.begin(), ZOrder.end(), wId);
    if (it != ZOrder.end())
    {
        ZOrder.erase(it);
        ZOrder.push_front(wId); // Move to the front of the Z-order
    }

    Render::Renderer->DamageGlobal(GetWindow(wId)->GetWindowRegion());
}

rpc_result_code_t WindowManagerClass::create_window(rpc_context_t *ctx, const CreateWindowRequest *req, CreateWindowResponse *resp)
{
    const auto clientContext = get_data<RpcClientContext>(ctx);

    const auto x = 100;
    const auto y = 100;

    const auto window = CreateWindow(req->title, Point(x, y), Size(req->size.width, req->size.height), WindowType::Regular);
    clientContext->windowIds.push_back(window->windowId);
    resp->window_id = window->windowId;
    resp->result.success = true;
    resp->result.error = nullptr;
    return RPC_RESULT_OK;
}

rpc_result_code_t WindowManagerClass::update_window_content(rpc_context_t *, const UpdateWindowContentRequest *req, UpdateWindowContentResponse *resp)
{
    const auto windowIt = windows.find(req->window_id);
    if (windowIt == windows.end())
    {
        resp->result.success = false;
        resp->result.error = strdup("Window not found");
        return RPC_RESULT_OK;
    }

    auto &window = windowIt->second;
    const auto region = Region(Point(req->region.x, req->region.y), Size(req->region.w, req->region.h));
    if (!window->UpdateContent(region, req->content->bytes, req->content->size))
    {
        resp->result.success = false;
        resp->result.error = strdup("Range out of bounds or invalid content");
        return RPC_RESULT_OK;
    }

    Render::Renderer->DamageGlobal(region.ToGlobal(window->GetPosition()));

    resp->result.success = true;
    resp->result.error = nullptr;
    return RPC_RESULT_OK;
}

void WindowManagerClass::on_connect(rpc_context_t *context)
{
    set_data(context, new RpcClientContext());
}

void WindowManagerClass::on_disconnect(rpc_context_t *context)
{
    auto clientContext = get_data<RpcClientContext>(context);
    if (clientContext)
    {
        for (const auto &windowId : clientContext->windowIds)
        {
            if (auto it = windows.find(windowId); it != windows.end())
            {
                auto window = it->second;
                std::cerr << "Removing window with ID: " << windowId << " on client disconnect." << std::endl;
                windows.erase(it);
                ZOrder.remove(windowId); // Remove from Z-order
                Render::Renderer->DamageGlobal(window->GetWindowRegion());
            }
        }
        // Clean up the client context
        delete clientContext;
    }
}
