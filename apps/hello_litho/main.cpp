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
#include "res_images.h"

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
        // DOWN: claim handler so Window captures us for MOVE/UP
        if (ev.action == TouchAction::DOWN) return true;
        if (ev.action == TouchAction::UP && mCb) {
            int col = ev.x / kCellW;
            int row = ev.y / kCellH;
            int idx = row * kCols + col;
            if (idx >= 0 && idx < IMG_COUNT) mCb(mUser, idx);
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

        int count = IMG_COUNT;
        if (count > kCols * kRows) count = kCols * kRows;

        for (int i = 0; i < count; i++) {
            int col = i % kCols;
            int row = i / kCols;
            int cx = col * kCellW + 14;
            int cy = row * kCellH;

            auto* icon = new ImageView(100, 100);
            icon->bounds() = {(int16_t)cx, (int16_t)cy, 100, 100};
            icon->setImageId((ImageId)i);
            root->addView(icon);
        }
    }
};

// -------- DetailActivity: feature showcase --------

class DetailActivity : public Activity {
public:
    void onCreate(Bundle& state) override {
        mIndex = state.getInt("index", 0);
        printf("DetailActivity::onCreate idx=%d\n", mIndex);

        auto* root = new ViewGroup();
        root->bounds() = {0, 0, 640, 480};
        setContentView(root);

        auto* bg = new ColorView(RGB565::fromRGB(32, 32, 48));
        bg->bounds() = {0, 0, 640, 480};
        root->addView(bg);

        int bw = 100, bh = 100;
        int y1 = 60, y2 = 220;

        // [0] Original RGBA
        addImage(root, 50,  y1, bw, bh, (ImageId)mIndex);

        // [1] Grayscale + red tint (A8 = alpha通道，红色填充)
        addImage(root, 210, y1, bw, bh, IMG_G_MUSIC)
            ->setTintColor(RGB565::fromRGB(255, 80, 80));

        // [2] 90° rotation (pivot at 0,0 → output fits within bounds)
        addImage(root, 370, y1, bw, bh, (ImageId)mIndex)
            ->setRotationAngle(90);

        // [3] Gold tint on RGBA (50% blend)
        addImage(root, 50,  y2, bw, bh, (ImageId)mIndex)
            ->setTintColor(RGB565::fromRGB(255, 200, 60));

        // [4] Continuous animated rotation (center pivot)
        addImage(root, 210, y2, bw, bh, (ImageId)mIndex);
        mImages[4]->setTranslationY(80);

        // [5] Grayscale + 180° + blue tint
        addImage(root, 370, y2, bw, bh, IMG_G_MUSIC)
            ->setTintColor(RGB565::fromRGB(100, 200, 255));
        mImages[5]->setRotationAngle(180);

        // Back button
        auto* backBtn = new Button(RGB565::fromRGB(200, 80, 80), 120, 40);
        backBtn->bounds() = {260, 400, 120, 40};
        backBtn->setOnClick([](void* self) {
            auto* a = (DetailActivity*)self;
            Intent intent;
            intent.target = "GalleryActivity";
            a->startActivity(intent);
        }, this);
        root->addView(backBtn);
        mBackBtn = backBtn;
    }

    void onResume() override {
        Activity::onResume();

        // Slide image [4] up
        if (mImages[4]) {
            mImages[4]->setTranslationY(80);
            mImages[4]->animate()
                .translationY(0)
                .setDuration(300)
                .setInterpolator(Interpolator::ACCELERATE_DECELERATE)
                .start(manager().windowManager().animationManager());
        }

        // ── continuous rotation: 0→360°, center pivot, infinite loop ──
        if (mImages[4]) {
            auto& am = manager().windowManager().animationManager();
            mRotAnim.setTarget(mImages[4])
                .setFloatValues(0, 360)
                .setSetter([](void* target, float val) {
                    ((ImageView*)target)->setRotationAngle((int16_t)val);
                })
                .setDuration(2000)
                .setInterpolator(Interpolator::LINEAR);
            mRotAnim.animator().setRepeatCount(-1);
            mRotAnim.start();
            am.addAnimator(&mRotAnim.animator());
        }
    }

private:
    ImageView* addImage(ViewGroup* parent, int x, int y, int w, int h,
                        ImageId id) {
        auto* v = new ImageView(w, h);
        v->bounds() = {(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h};
        v->setImageId(id);
        parent->addView(v);
        for (int i = 0; i < 6; i++) {
            if (!mImages[i]) { mImages[i] = v; break; }
        }
        return v;
    }

    int        mIndex    = 0;
    ImageView* mImages[6] = {};
    Button*    mBackBtn  = nullptr;
    ObjectAnimator mRotAnim;
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

    // Start directly on the feature showcase with the first icon
    Intent startIntent;
    startIntent.target = "DetailActivity";
    startIntent.putInt("index", 0);
    am.startActivity(startIntent);

    printf("LithoUI — running (%d images)\n", IMG_COUNT);
    wm.run();
    printf("LithoUI — done\n");
    return 0;
}
