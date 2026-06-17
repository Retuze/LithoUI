#include "core/litho_core.h"
#include "core/tile.hpp"
#include "core/region.hpp"
#include "core/painter.hpp"

#include "framework/view/view.hpp"
#include "framework/view/view_group.hpp"
#include "framework/window/window.hpp"
#include "framework/window/window_manager.hpp"
#include "framework/activity/activity.hpp"
#include "framework/activity/activity_manager.hpp"
#include "framework/intent/intent.hpp"

#include "port/display_adapter.hpp"
#include "port/input_adapter.hpp"
#include "port/tick_adapter.hpp"
#include "port/x11/x11_display.hpp"
#include "port/x11/x11_input.hpp"
#include "port/x11/x11_tick.hpp"

#include <cstdio>

using namespace litho;

// -------- touch-aware rect view --------

class RectView : public View {
public:
    RectView(RGB565 color, int w, int h)
        : mColor(color), mPressedColor(color) {
        // darken for pressed state
        uint8_t r = (color.value >> 11) & 0x1F;
        uint8_t g = (color.value >> 5)  & 0x3F;
        uint8_t b =  color.value        & 0x1F;
        mPressedColor = RGB565::fromRGB(r * 128 / 31, g * 64 / 63, b * 128 / 31);  // half bright
    }

    void onDraw(Painter& p) override {
        RGB565 c = mPressed ? mPressedColor : mColor;
        p.fillRect(0, 0, mBounds.width, mBounds.height, c);
        if (mDrawCount == 0) {
            printf( "DRAW %p: (%d,%d,%d,%d) c=%04x\n",
                   (void*)this, mBounds.x, mBounds.y,
                   mBounds.width, mBounds.height, c.value);
        }
        mDrawCount++;
    }

    bool onTouchEvent(TouchEvent& ev) override {
        if (ev.action == TouchAction::DOWN) {
            if (!mCallback) return false;
            if (!mPressed) {
                mPressed = true;
                invalidate();   // visual state changed
            }
            mCallback(mUser);
            return true;
        }
        if (ev.action == TouchAction::UP) {
            if (!mCallback) return false;
            if (mPressed) {
                mPressed = false;
                invalidate();
            }
            return true;
        }
        return false;
    }

    int mDrawCount = 0;

    void setOnClick(void (*cb)(void*), void* user) { mCallback = cb; mUser = user; }

private:
    RGB565 mColor;
    RGB565 mPressedColor;
    bool   mPressed  = false;
    void (*mCallback)(void*) = nullptr;
    void* mUser = nullptr;
};

// -------- MainActivity (green) --------

class MainActivity : public Activity {
public:
    void onCreate(Bundle& state) override {
        (void)state;
        printf("MainActivity::onCreate\n");

        auto* root = new ViewGroup();
        root->bounds() = {0, 0, 640, 480};
        setContentView(root);

        auto* bg = new RectView(RGB565::Green(), 640, 480);
        bg->bounds() = {0, 0, 640, 480};
        root->addView(bg);

        auto* btn = new RectView(RGB565::Red(), 120, 48);
        btn->bounds() = {260, 216, 120, 48};
        btn->setOnClick([](void* self) {
            auto* a = (MainActivity*)self;
            Intent intent;
            intent.target = "SecondActivity";
            intent.putInt("from", 1);
            a->startActivity(intent);
        }, this);
        root->addView(btn);
    }
};

// -------- SecondActivity (blue) --------

class SecondActivity : public Activity {
public:
    void onCreate(Bundle& state) override {
        int from = state.getInt("from", 0);
        printf("SecondActivity::onCreate (from=%d)\n", from);

        auto* root = new ViewGroup();
        root->bounds() = {0, 0, 640, 480};
        setContentView(root);

        auto* bg = new RectView(RGB565::Blue(), 640, 480);
        bg->bounds() = {0, 0, 640, 480};
        root->addView(bg);

        auto* btn = new RectView(RGB565::Red(), 120, 48);
        btn->bounds() = {260, 216, 120, 48};
        btn->setOnClick([](void* self) {
            ((SecondActivity*)self)->finish();
        }, this);
        root->addView(btn);
    }
};

// -------- entry --------

int main() {
    printf("LithoUI — hello_litho\n");

    X11Display display;
    if (!display.init(640, 480)) {
        printf( "FATAL: display init failed\n");
        return 1;
    }

    X11Input input(display);
    X11Tick  tick;
    WindowManager wm(display, input, tick);
    wm.initPFB(128, 4, 4);

    ActivityManager am(wm);

    // Register activities — callers only need the name string.
    am.registerActivity<MainActivity>("MainActivity");
    am.registerActivity<SecondActivity>("SecondActivity");

    // Launch main
    Intent startIntent;
    startIntent.target = "MainActivity";
    am.startActivity(startIntent);

    printf("LithoUI — running\n");
    wm.run();
    printf("LithoUI — done\n");
    return 0;
}
