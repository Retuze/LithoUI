#pragma once
#include "port/input_adapter.hpp"
#include "port/x11/x11_display.hpp"

#include <X11/Xlib.h>
#include <X11/keysym.h>

namespace litho {

class X11Input : public InputAdapter {
public:
    explicit X11Input(X11Display& disp)
        : mDisplay(disp.xDisplay()), mXWindow(disp.xWindow()) {}

    bool pollEvent(Event& out) override {
        if (!XPending(mDisplay)) {
            out.type = EventType::NONE;
            return false;
        }

        XEvent xev;
        XNextEvent(mDisplay, &xev);

        switch (xev.type) {
        case ButtonPress:
            out.type        = EventType::TOUCH;
            out.touch.x     = xev.xbutton.x;
            out.touch.y     = xev.xbutton.y;
            out.touch.action = TouchAction::DOWN;
            return true;

        case ButtonRelease:
            out.type        = EventType::TOUCH;
            out.touch.x     = xev.xbutton.x;
            out.touch.y     = xev.xbutton.y;
            out.touch.action = TouchAction::UP;
            return true;

        case MotionNotify:
            out.type        = EventType::TOUCH;
            out.touch.x     = xev.xmotion.x;
            out.touch.y     = xev.xmotion.y;
            out.touch.action = TouchAction::MOVE;
            return true;

        case KeyPress:
        case KeyRelease: {
            KeySym ks = XLookupKeysym(&xev.xkey, 0);
            out.type = EventType::KEY;
            out.key.code   = translateKey(ks);
            out.key.action = (xev.type == KeyPress)
                             ? KeyAction::DOWN : KeyAction::UP;
            return true;
        }

        case ClientMessage:
            out.type = EventType::QUIT;
            return true;
        }

        out.type = EventType::NONE;
        return false;
    }

private:
    static KeyCode translateKey(KeySym ks) {
        switch (ks) {
        case XK_Escape:    return KeyCode::ESC;
        case XK_Return:    return KeyCode::ENTER;
        case XK_space:     return KeyCode::SPACE;
        case XK_Left:      return KeyCode::LEFT;
        case XK_Right:     return KeyCode::RIGHT;
        case XK_Up:        return KeyCode::UP;
        case XK_Down:      return KeyCode::DOWN;
        default:           return KeyCode::NONE;
        }
    }

    Display*  mDisplay;
    ::Window  mXWindow;
};

} // namespace litho
