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
#include "generated/res_bundle.h"

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
#include <cstring>

using namespace litho;

static const int kCols = 5;
static const int kRows = 5;
static const int kCellW = 128;
static const int kCellH = 96;

// -------- simple color background --------

class ColorView : public View {
public:
    explicit ColorView(RGB565 color) : mColor(color) {}
    void onDraw(Painter& p) override {
        p.fillRect(0, 0, mBounds.width, mBounds.height, mColor);
    }
private:
    RGB565 mColor;
};

// -------- GalleryActivity: icon grid --------

class IconGrid : public ViewGroup {
public:
    void setOnIconClick(void (*cb)(void*, int), void* user) { mCb = cb; mUser = user; }

    bool onTouchEvent(TouchEvent& ev) override {
        if (ev.action == TouchAction::UP && mCb) {
            int col = ev.x / kCellW;
            int row = ev.y / kCellH;
            int idx = row * kCols + col;
            if (idx >= 0 && idx < kImageAssetCount) mCb(mUser, idx);
            return true;
        }
        return false;
    }

private:
    void (*mCb)(void*, int) = nullptr;
    void* mUser = nullptr;
};

class GalleryActivity : public Activity {
public:
    void onCreate(Bundle& state) override {
        (void)state;
        printf("GalleryActivity::onCreate\n");

        auto* root = new IconGrid();
        root->bounds() = {0, 0, 640, 480};
        root->setOnIconClick([](void* self, int idx) {
            auto* a = (GalleryActivity*)self;
            Intent intent;
            intent.target = "DetailActivity";
            intent.putInt("index", idx);
            a->startActivity(intent);
        }, this);
        setContentView(root);

        auto* bg = new ColorView(RGB565::fromRGB(32, 32, 48));
        bg->bounds() = {0, 0, 640, 480};
        root->addView(bg);

        int count = kImageAssetCount;
        if (count > kCols * kRows) count = kCols * kRows;

        for (int i = 0; i < count; i++) {
            int col = i % kCols;
            int row = i / kCols;
            int cx = col * kCellW + 14;
            int cy = row * kCellH;

            auto* icon = new ImageView(100, 100);
            icon->bounds() = {(int16_t)cx, (int16_t)cy, 100, 100};
            icon->setImage(&kImageAssets[i]);
            root->addView(icon);
        }
    }
};

// -------- DetailActivity: single icon large --------

class DetailActivity : public Activity {
public:
    void onCreate(Bundle& state) override {
        mIndex = state.getInt("index", 0);
        printf("DetailActivity::onCreate idx=%d (%s)\n", mIndex,
               mIndex < kImageAssetCount ? kImageAssets[mIndex].name : "?");

        auto* root = new ViewGroup();
        root->bounds() = {0, 0, 640, 480};
        setContentView(root);

        auto* bg = new ColorView(RGB565::fromRGB(32, 32, 48));
        bg->bounds() = {0, 0, 640, 480};
        root->addView(bg);

        // Icon centered
        if (mIndex < kImageAssetCount) {
            auto* icon = new ImageView(100, 100);
            icon->bounds() = {270, 120, 100, 100};
            icon->setImage(&kImageAssets[mIndex]);

            // Slide in from below
            icon->setTranslationY(80);
            root->addView(icon);
            mIcon = icon;
        }

        // Name label
        auto* label = new TextView(200, 28);
        label->bounds() = {220, 240, 200, 28};
        root->addView(label);
        mLabel = label;

        // Back button
        auto* backBtn = new Button(RGB565::fromRGB(200, 80, 80), 120, 40);
        backBtn->bounds() = {260, 380, 120, 40};
        backBtn->setOnClick([](void* self) {
            ((DetailActivity*)self)->finish();
        }, this);
        backBtn->setTranslationY(60);
        root->addView(backBtn);
        mBackBtn = backBtn;
    }

    void onResume() override {
        Activity::onResume();
        if (mIcon) {
            mIcon->setTranslationY(80);
            mIcon->animate()
                .translationY(0)
                .setDuration(300)
                .setInterpolator(Interpolator::ACCELERATE_DECELERATE)
                .start(manager().windowManager().animationManager());
        }
        if (mBackBtn) {
            mBackBtn->setTranslationY(60);
            mBackBtn->animate()
                .translationY(0)
                .setDuration(350)
                .setInterpolator(Interpolator::ACCELERATE_DECELERATE)
                .start(manager().windowManager().animationManager());
        }
    }

private:
    int        mIndex = 0;
    ImageView* mIcon = nullptr;
    TextView*  mLabel = nullptr;
    Button*    mBackBtn = nullptr;
};

// -------- entry --------

int main() {
#ifdef _WIN32
    printf("LithoUI — gallery (GDI)\n");
    GdiDisplay display;
    if (!display.init(640, 480)) {
        printf("FATAL: display init failed\n");
        return 1;
    }
    GdiInput input(display);
    GdiTick  tick;
#else
    printf("LithoUI — gallery (X11)\n");
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
    am.registerActivity<GalleryActivity>("GalleryActivity");
    am.registerActivity<DetailActivity>("DetailActivity");

    Intent startIntent;
    startIntent.target = "GalleryActivity";
    am.startActivity(startIntent);

    printf("LithoUI — running (%d images)\n", kImageAssetCount);
    wm.run();
    printf("LithoUI — done\n");
    return 0;
}
