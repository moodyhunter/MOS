// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "input/input.hpp"
#include "proto/graphics-dm.service.h"
#include "windows/window.hpp"

#include <atomic>
#include <list>
#include <map>
#include <memory>

namespace DisplayManager::Windows
{
    constexpr auto WINDOW_MANAGER_SERVICE_NAME = "mos.window-manager";

    class WindowManagerClass : public IWindowManagerService
    {
      public:
        explicit WindowManagerClass();
        ~WindowManagerClass();

      public:
        std::shared_ptr<Window> GetWindow(u64 windowId)
        {
            auto it = windows.find(windowId);
            if (it != windows.end())
                return it->second;
            return nullptr;
        }

        std::shared_ptr<Window> CreateWindow(const std::string &name, Point pos, Size size, WindowType type = WindowType::Regular);

        Delta MoveWindowTo(u64 wId, Point newPosition);

        void BringWindowToFront(u64 wId);

        void DispatchMouseEvent(const Input::MouseEvent &event);

      private:
        void on_connect(rpc_context_t *context) override;
        void on_disconnect(rpc_context_t *context) override;

      private:
        rpc_result_code_t create_window(rpc_context_t *context, const CreateWindowRequest *req, CreateWindowResponse *resp) override;
        rpc_result_code_t update_window_content(rpc_context_t *context, const UpdateWindowContentRequest *req, UpdateWindowContentResponse *resp) override;

      private:
        std::atomic<u64> nextWindowId = 0x1000; ///< Next window ID to be assigned

      private:
        std::map<u64, std::shared_ptr<Window>> windows; ///< Map of window IDs to Window objects
    };

    inline std::list<u64> ZOrder;         ///< List of window IDs in Z-order (from topmost to bottommost)
    inline std::list<u64> TopMostWindows; ///< List of topmost window IDs
    inline const std::unique_ptr<WindowManagerClass> WindowManager = std::make_unique<WindowManagerClass>();
} // namespace DisplayManager::Windows
