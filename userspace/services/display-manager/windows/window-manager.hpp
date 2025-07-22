// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "proto/graphics-dm.service.h"
#include "windows/window.hpp"

#include <atomic>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <vector>

namespace DisplayManager::Windows
{
    constexpr auto WINDOW_MANAGER_SERVICE_NAME = "mos.window-manager";

    class WindowManagerClass : public IWindowManagerService
    {
      public:
        WindowManagerClass();
        ~WindowManagerClass();

      public:
        std::shared_ptr<Window> GetWindow(u64 windowId)
        {
            auto it = windows.find(windowId);
            if (it != windows.end())
                return it->second;
            return nullptr;
        }

      private:
        void on_connect(rpc_context_t *context) override
        {
            std::cerr << "WindowManager: Client connected." << std::endl;
        }

        void on_disconnect(rpc_context_t *context) override
        {
            std::cerr << "WindowManager: Client disconnected." << std::endl;
        }

      private:
        rpc_result_code_t create_window(rpc_context_t *context, const CreateWindowRequest *req, CreateWindowResponse *resp) override;
        rpc_result_code_t update_window_content(rpc_context_t *context, const UpdateWindowContentRequest *req, UpdateWindowContentResponse *resp) override;

      private:
        std::atomic<u64> nextWindowId = 1; ///< Next window ID to be assigned

      private:
        std::map<u64, std::shared_ptr<Window>> windows; ///< Map of window IDs to Window objects
    };

    inline std::list<u64> ZOrder; ///< List of window IDs in Z-order (from topmost to bottommost)
    inline std::unique_ptr<WindowManagerClass> WindowManager = std::make_unique<WindowManagerClass>();
} // namespace DisplayManager::Windows
