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
#include <cmath>
#include <ctime>

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

// -------- ClockActivity: analog clock with sweeping hands --------

static const int kClockCX = 320, kClockCY = 240, kFaceR = 150;

// Face: dark dial, rim, 12 ticks, drawn relative to view origin.
class ClockFaceView : public View {
public:
    void onDraw(Painter& p) override {
        int cx = mBounds.width / 2, cy = mBounds.height / 2;
        RGB565 face = RGB565::fromRGB(34, 38, 52);
        RGB565 rim  = RGB565::fromRGB(90, 100, 130);
        RGB565 tick = RGB565::fromRGB(210, 215, 230);

        // filled dial (scanline circle) + rim
        for (int dy = -kFaceR; dy <= kFaceR; dy++) {
            int half = (int)(sqrt((double)(kFaceR*kFaceR - dy*dy)) + 0.5);
            p.fillRect(cx - half, cy + dy, 2*half, 1, face);
        }
        for (int dy = -kFaceR; dy <= kFaceR; dy++) {
            int ho = (int)(sqrt((double)(kFaceR*kFaceR - dy*dy)) + 0.5);
            int hi = 0;
            int inr = kFaceR - 4;
            if (dy > -inr && dy < inr) hi = (int)(sqrt((double)(inr*inr - dy*dy)) + 0.5);
            p.fillRect(cx - ho, cy + dy, ho - hi, 1, rim);
            p.fillRect(cx + hi, cy + dy, ho - hi, 1, rim);
        }
        // 12 hour ticks
        for (int k = 0; k < 12; k++) {
            double a = k * 3.14159265 / 6.0;
            int tx = cx + (int)(132 * sin(a));
            int ty = cy - (int)(132 * cos(a));
            int s = (k % 3 == 0) ? 5 : 3;
            p.fillRect(tx - s, ty - s, 2*s, 2*s, tick);
        }
    }
};

// Center hub is the black dot baked into the hand sprites — no extra hub.

class ClockActivity : public Activity {
public:
    void onCreate(Bundle& state) override {
        (void)state;
        printf("ClockActivity::onCreate\n");

        auto* root = new ViewGroup();
        root->bounds() = {0, 0, 640, 480};
        setContentView(root);

        auto* bg = new ColorView(RGB565::fromRGB(18, 20, 26));
        bg->bounds() = {0, 0, 640, 480};
        root->addView(bg);

        auto* face = new ClockFaceView();
        face->bounds() = {(int16_t)(kClockCX - kFaceR - 6),
                          (int16_t)(kClockCY - kFaceR - 6),
                          (int16_t)(2*(kFaceR + 6)), (int16_t)(2*(kFaceR + 6))};
        root->addView(face);

        // Hands: view == image size, pivoted on the black dot baked into
        // each sprite (sec ≈ (7,108), min ≈ (8,126)), placed so that dot
        // lands on the clock centre. View==image means the dirty AABB and
        // the painter agree, so the pivot stays pinned at every angle.
        mMin = makeHand(root, IMG_R_MIN, 8, 126, nullptr);
        RGB565 red = RGB565::fromRGB(220, 70, 70);
        mSec = makeHand(root, IMG_R_SEC, 7, 108, &red);

        // Initial angles from wall-clock time so it shows the real time.
        time_t t = time(nullptr);
        struct tm lt = *localtime(&t);
        mSec0 = lt.tm_sec * 6;
        mMin0 = (int)((lt.tm_min + lt.tm_sec / 60.0) * 6.0);
        if (mMin) mMin->setRotationAngle((int16_t)mMin0, 8, 126);
        if (mSec) mSec->setRotationAngle((int16_t)mSec0, 7, 108);
        printf("clock start %02d:%02d:%02d (min=%d° sec=%d°)\n",
               lt.tm_hour, lt.tm_min, lt.tm_sec, mMin0, mSec0);
    }

    void onResume() override {
        Activity::onResume();
        auto& am = manager().windowManager().animationManager();

        // Second hand: one revolution per 60 s, forever. Driven in
        // deci-degrees (×10) so the sweep is sub-degree smooth.
        if (mSec) {
            mSecAnim.setTarget(mSec)
                .setFloatValues((float)mSec0, (float)mSec0 + 360.f)
                .setSetter([](void* t, float v){ ((ImageView*)t)->setRotationAngleDeci((int)(v*10.f + 0.5f)); })
                .setDuration(60000)
                .setInterpolator(Interpolator::LINEAR);
            mSecAnim.animator().setRepeatCount(-1);
            mSecAnim.start();
            am.addAnimator(&mSecAnim.animator());
        }
        // Minute hand: one revolution per 60 min.
        if (mMin) {
            mMinAnim.setTarget(mMin)
                .setFloatValues((float)mMin0, (float)mMin0 + 360.f)
                .setSetter([](void* t, float v){ ((ImageView*)t)->setRotationAngleDeci((int)(v*10.f + 0.5f)); })
                .setDuration(3600000)
                .setInterpolator(Interpolator::LINEAR);
            mMinAnim.animator().setRepeatCount(-1);
            mMinAnim.start();
            am.addAnimator(&mMinAnim.animator());
        }
    }

private:
    ImageView* makeHand(ViewGroup* root, ImageId id, int pivotX, int pivotY,
                        const RGB565* tint) {
        const ImageEntry* e = imageEntry(id);
        auto* v = new ImageView(e->width, e->height);   // view == image size
        // Place so the hand's own pivot (the black dot baked into the art)
        // lands on the clock centre.
        v->bounds() = {(int16_t)(kClockCX - pivotX), (int16_t)(kClockCY - pivotY),
                       (int16_t)e->width, (int16_t)e->height};
        v->setImageId(id);
        if (tint) v->setTintColor(*tint);
        root->addView(v);
        return v;
    }

    ImageView* mMin = nullptr;
    ImageView* mSec = nullptr;
    int mMin0 = 0, mSec0 = 0;
    ObjectAnimator mSecAnim, mMinAnim;
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
    wm.initPFB(128, 128, 2);

    ActivityManager am(wm);
    am.registerActivity<GalleryActivity>("GalleryActivity");
    am.registerActivity<DetailActivity>("DetailActivity");
    am.registerActivity<ClockActivity>("ClockActivity");

    // Start on the analog clock
    Intent startIntent;
    startIntent.target = "ClockActivity";
    am.startActivity(startIntent);

    printf("LithoUI — running (%d images)\n", IMG_COUNT);
    wm.run();
    printf("LithoUI — done\n");
    return 0;
}
