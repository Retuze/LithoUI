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
#include "framework/widget/button.hpp"
#include "framework/widget/text_view.hpp"
#include "framework/widget/image_view.hpp"
#include "framework/animation/view_property_animator.hpp"
#include "framework/animation/animation_manager.hpp"

#include "port/display_adapter.hpp"
#include "port/input_adapter.hpp"
#include "port/tick_adapter.hpp"

#ifdef _WIN32
#include "port/gdi/gdi_display.hpp"
#include "port/gdi/gdi_input.hpp"
#include "port/gdi/gdi_tick.hpp"
#else
#include "port/x11/x11_display.hpp"
#include "port/x11/x11_input.hpp"
#include "port/x11/x11_tick.hpp"
#endif

#include <cstdio>

using namespace litho;

// -------- simple color background (no touch) --------

class ColorView : public View {
public:
    explicit ColorView(RGB565 color) : mColor(color) {}

    void onDraw(Painter& p) override {
        p.fillRect(0, 0, mBounds.width, mBounds.height, mColor);
    }

private:
    RGB565 mColor;
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

        auto* bg = new ColorView(RGB565::Green());
        bg->bounds() = {0, 0, 640, 480};
        root->addView(bg);

        mBtn = new Button(RGB565::Red(), 120, 48);
        mBtn->bounds() = {260, 216, 120, 48};
        mBtn->setOnClick([](void* self) {
            auto* a = (MainActivity*)self;
            Intent intent;
            intent.target = "SecondActivity";
            intent.putInt("from", 1);
            a->startActivity(intent);
        }, this);
        root->addView(mBtn);

        auto* label = new TextView(120, 24);
        label->bounds() = {260, 180, 120, 24};
        root->addView(label);

        auto* icon = new ImageView(48, 48);
        icon->bounds() = {296, 300, 48, 48};
        root->addView(icon);
        mIcon = icon;

        auto* animBtn = new Button(RGB565::fromRGB(160, 160, 160), 80, 32);
        animBtn->bounds() = {280, 368, 80, 32};
        animBtn->setOnClick([](void* self) {
            auto* a = (MainActivity*)self;
            auto& mgr = a->manager().windowManager().animationManager();
            if (!a->mIconMoved) {
                a->mIcon->animate()
                    .translationY(-30)
                    .alpha(80)
                    .setDuration(350)
                    .setInterpolator(Interpolator::ACCELERATE_DECELERATE)
                    .start(mgr);
            } else {
                a->mIcon->animate()
                    .translationY(0)
                    .alpha(255)
                    .setDuration(350)
                    .setInterpolator(Interpolator::ACCELERATE_DECELERATE)
                    .start(mgr);
            }
            a->mIconMoved = !a->mIconMoved;
        }, this);
        root->addView(animBtn);
    }

    void onResume() override {
        Activity::onResume();
        mBtn->setTranslationX(120);  // reset offscreen before slide-in
        mBtn->animate()
            .translationX(0)
            .setDuration(400)
            .setInterpolator(Interpolator::ACCELERATE_DECELERATE)
            .start(manager().windowManager().animationManager());
    }

private:
    Button*    mBtn       = nullptr;
    ImageView* mIcon      = nullptr;
    bool       mIconMoved = false;
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

        auto* bg = new ColorView(RGB565::Blue());
        bg->bounds() = {0, 0, 640, 480};
        root->addView(bg);

        auto* btn = new Button(RGB565::Red(), 120, 48);
        btn->bounds() = {260, 216, 120, 48};
        btn->setOnClick([](void* self) {
            ((SecondActivity*)self)->finish();
        }, this);
        root->addView(btn);

        auto* label = new TextView(120, 24);
        label->bounds() = {260, 180, 120, 24};
        root->addView(label);
    }
};

// -------- entry --------

int main() {
#ifdef _WIN32
    printf("LithoUI — hello_litho (GDI)\n");

    GdiDisplay display;
    if (!display.init(640, 480)) {
        printf("FATAL: display init failed\n");
        return 1;
    }

    GdiInput input(display);
    GdiTick  tick;
#else
    printf("LithoUI — hello_litho (X11)\n");

    X11Display display;
    if (!display.init(640, 480)) {
        printf("FATAL: display init failed\n");
        return 1;
    }

    X11Input input(display);
    X11Tick  tick;
#endif

    WindowManager wm(display, input, tick);
    wm.initPFB(128, 4, 2);

    ActivityManager am(wm);

    am.registerActivity<MainActivity>("MainActivity");
    am.registerActivity<SecondActivity>("SecondActivity");

    Intent startIntent;
    startIntent.target = "MainActivity";
    am.startActivity(startIntent);

    printf("LithoUI — running\n");
    wm.run();
    printf("LithoUI — done\n");
    return 0;
}
