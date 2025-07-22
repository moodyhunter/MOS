// SPDX-License-Identifier: GPL-3.0-or-later

#include "windows/window-manager.hpp"

#include "proto/graphics-dm.service.h"
#include "render/renderer.hpp"

#include <iostream>
#include <librpc/rpc.h>

using namespace DisplayManager::Windows;

WindowManagerClass::WindowManagerClass() : IWindowManagerService(WINDOW_MANAGER_SERVICE_NAME)
{
}

WindowManagerClass::~WindowManagerClass()
{
}

rpc_result_code_t WindowManagerClass::create_window(rpc_context_t *, const CreateWindowRequest *req, CreateWindowResponse *resp)
{
    const auto windowId = nextWindowId++;
    std::cerr << "Creating window with ID: " << windowId << ", Title: " << req->display_name << ", Size: " << req->width << "x" << req->height << std::endl;
    const auto window = std::make_shared<Window>(windowId, req->display_name, Point(80 * windowId, 80 * windowId), Size(req->width, req->height));
    windows[window->windowId] = window;
    resp->window_id = window->windowId;
    resp->result.success = true;
    resp->result.error = nullptr;
    ZOrder.push_front(window->windowId); // Add to the front of Z-order (topmost)
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
    if (!window->UpdateContent(region, req->content))
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
