#pragma once
#include "port/input_adapter.hpp"
#include "port/gdi/gdi_display.hpp"

#include <windows.h>
#include <windowsx.h>

namespace litho {

class GdiInput : public InputAdapter {
public:
    explicit GdiInput(GdiDisplay& disp)
        : mHwnd(disp.hwnd()) {}

    bool pollEvent(Event& out) override {
        MSG msg;

        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                out.type = EventType::QUIT;
                return true;
            }

            switch (msg.message) {
            case WM_LBUTTONDOWN:
                out.type        = EventType::TOUCH;
                out.touch.x     = GET_X_LPARAM(msg.lParam);
                out.touch.y     = GET_Y_LPARAM(msg.lParam);
                out.touch.action = TouchAction::DOWN;
                return true;

            case WM_LBUTTONUP:
                out.type        = EventType::TOUCH;
                out.touch.x     = GET_X_LPARAM(msg.lParam);
                out.touch.y     = GET_Y_LPARAM(msg.lParam);
                out.touch.action = TouchAction::UP;
                return true;

            case WM_MOUSEMOVE:
                out.type        = EventType::TOUCH;
                out.touch.x     = GET_X_LPARAM(msg.lParam);
                out.touch.y     = GET_Y_LPARAM(msg.lParam);
                out.touch.action = TouchAction::MOVE;
                return true;

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                out.type = EventType::KEY;
                out.key.code   = translateKey((UINT)msg.wParam);
                out.key.action = KeyAction::DOWN;
                return true;

            case WM_KEYUP:
            case WM_SYSKEYUP:
                out.type = EventType::KEY;
                out.key.code   = translateKey((UINT)msg.wParam);
                out.key.action = KeyAction::UP;
                return true;

            case WM_CLOSE:
                out.type = EventType::QUIT;
                return true;

            default:
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                break;
            }
        }

        out.type = EventType::NONE;
        return false;
    }

private:
    static KeyCode translateKey(UINT vk) {
        switch (vk) {
        case VK_ESCAPE: return KeyCode::ESC;
        case VK_RETURN: return KeyCode::ENTER;
        case VK_SPACE:  return KeyCode::SPACE;
        case VK_LEFT:   return KeyCode::LEFT;
        case VK_RIGHT:  return KeyCode::RIGHT;
        case VK_UP:     return KeyCode::UP;
        case VK_DOWN:   return KeyCode::DOWN;
        default:        return KeyCode::NONE;
        }
    }

    HWND mHwnd;
};

} // namespace litho
