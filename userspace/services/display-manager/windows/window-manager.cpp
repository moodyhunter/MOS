// SPDX-License-Identifier: GPL-3.0-or-later

#include "windows/window-manager.hpp"

#include "proto/graphics-dm.pb.h"
#include "proto/graphics-dm.service.h"
#include "proto/input-types.pb.h"
#include "render/renderer.hpp"
#include "utils/SubView.hpp"
#include "utils/common.hpp"
#include "windows/window.hpp"

#include <iostream>
#include <librpc/rpc.h>
#include <pb.h>
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
        auto w = GetWindow(wId);
        if (!w)
            continue; // Skip if window not found

        if (w->windowType == WindowType::Cursor || w->windowType == WindowType::Background)
            continue;

        // Check if the mouse event is within the window's region
        const auto region = w->GetWindowRegion();
        if (region.Test(event.cursorPosition))
        {
            if (w->HandleMouseEvent(event))
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
    const auto wType = req->specialType == SpecialWindowType::SpecialWindowType_Desktop ? WindowType::Desktop : WindowType::Regular;

    const auto x = 100;
    const auto y = 100;

    const auto window = CreateWindow(req->title, Point(x, y), Size(req->size.width, req->size.height), wType);
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

rpc_result_code_t WindowManagerClass::move_window(rpc_context_t *, const MoveWindowRequest *req, MoveWindowResponse *resp)
{
    MoveWindowTo(req->window_id, Point{ static_cast<int>(req->x), static_cast<int>(req->y) });
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

rpc_result_code_t WindowManagerClass::get_window_list(rpc_context_t *, const GetWindowListRequest *, GetWindowListResponse *resp)
{
    resp->windows_count = static_cast<u32>(windows.size());
    resp->windows = static_cast<WindowInfo *>(malloc(resp->windows_count * sizeof(WindowInfo)));
    if (!resp->windows)
    {
        resp->result.success = false;
        resp->result.error = strdup("Memory allocation failed");
        return RPC_RESULT_OK;
    }

    u32 index = 0;
    for (const auto &pair : windows)
    {
        const auto &window = pair.second;
        resp->windows[index].window_id = window->windowId;
        resp->windows[index].title = strdup(window->title.c_str());
        resp->windows[index].bounds.x = window->GetWindowRegion().origin.x;
        resp->windows[index].bounds.y = window->GetWindowRegion().origin.y;
        resp->windows[index].bounds.w = window->GetWindowRegion().size.width;
        resp->windows[index].bounds.h = window->GetWindowRegion().size.height;
        index++;
    }

    resp->result.success = true;
    resp->result.error = nullptr;
    return RPC_RESULT_OK;
}

extern void *renderBuffer;

rpc_result_code_t WindowManagerClass::do_screenshot(rpc_context_t *, const ScreenshotRequest *req, ScreenshotResponse *resp)
{
    if (req->window_id == 0)
    {
        // screenshot entire screen
        const auto screenSize = Render::Renderer->GetDisplaySize();
        const auto bufsize = screenSize.width * screenSize.height * 4;
        pb_bytes_array_t *content = static_cast<pb_bytes_array_t *>(malloc(PB_BYTES_ARRAY_T_ALLOCSIZE(bufsize)));
        content->size = bufsize;
        resp->size.width = screenSize.width;
        resp->size.height = screenSize.height;
        resp->image = content;
        memcpy(content->bytes, renderBuffer, bufsize);
    }
    else
    {
        auto window = GetWindow(req->window_id);
        if (!window)
        {
            resp->result.success = false;
            resp->result.error = strdup("Window not found");
            return RPC_RESULT_OK;
        }

        // Capture screenshot of the specific window
        const auto region = window->GetWindowRegion();
        const auto bufsize = region.size.width * region.size.height * 4;
        pb_bytes_array_t *content = static_cast<pb_bytes_array_t *>(malloc(PB_BYTES_ARRAY_T_ALLOCSIZE(bufsize)));
        content->size = bufsize;

        Utils::SubView<uint32_t> destination(content->bytes, bufsize, region.size, Region{ {}, region.size });
        window->GetRegionContent(region, destination);

        resp->image = content;
        resp->size.width = region.size.width;
        resp->size.height = region.size.height;
    }

    resp->result.success = true;
    resp->result.error = nullptr;
    return RPC_RESULT_OK;
}

rpc_result_code_t WindowManagerClass::query_display_info(rpc_context_t *, const QueryDisplayInfoRequest *, QueryDisplayInfoResponse *resp)
{
    auto size = Render::Renderer->GetDisplaySize();
    resp->size.width = size.width;
    resp->size.height = size.height;
    resp->result.success = true;
    resp->result.error = nullptr;
    return RPC_RESULT_OK;
}

rpc_result_code_t WindowManagerClass::handle_event(rpc_context_t *, const HandleEventRequest *req, HandleEventResponse *resp)
{
    const auto window = GetWindow(req->window_id);
    if (!window)
    {
        resp->result.error = strdup("failed to get window");
        resp->result.success = false;
        return RPC_RESULT_OK;
    }

    const auto event = window->WaitForMouseEvent();
    resp->event_data.type = input_InputEventType_MOUSE_MOVE;
    resp->event_data.which_event_type = input_InputEvent_mouse_move_tag;
    resp->event_data.event_type.mouse_move.position.x = event.cursorPosition.x;
    resp->event_data.event_type.mouse_move.position.y = event.cursorPosition.y;
    resp->result.success = true;
    resp->result.error = nullptr;
    return RPC_RESULT_OK;
}
