/**
 * demo_render.cpp — Render the hello_litho scene to a PPM file
 *
 * Uses the actual View/Activity framework but renders to a memory
 * tile instead of X11.  Writes output as PPM (text-based, no lib needed).
 */
#include "core/litho_core.h"
#include "core/tile.hpp"
#include "core/region.hpp"
#include "core/painter.hpp"
#include "core/dirty_list.hpp"
#include "core/pfb.hpp"

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
#include "port/tick_adapter.hpp"
#include "port/input_adapter.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>

using namespace litho;

// ── Memory-backed display adapter ───────────────────────────────────

static uint16_t* gFramebuffer = nullptr;
static int gFBW = 0, gFBH = 0;

struct MemDisplay : public DisplayAdapter {
    bool init(int w, int h) override {
        gFBW = w; gFBH = h;
        gFramebuffer = new uint16_t[w * h];
        memset(gFramebuffer, 0, w * h * 2);
        return true;
    }
    void bitblt(const uint16_t* data, int x, int y, int w, int h) override {
        for (int dy = 0; dy < h; dy++)
            memcpy(gFramebuffer + (y+dy)*gFBW + x, data + dy*w, w*2);
    }
    void flush() override {}
    int width()  const override { return gFBW; }
    int height() const override { return gFBH; }
};

struct MemTick : public TickAdapter {
    uint32_t tickMs() override { return mMs; }
    void advance(uint32_t ms) { mMs += ms; }
    uint32_t mMs = 0;
};

struct MemInput : public InputAdapter {
    bool pollEvent(Event& out) override {
        if (mPendingCount > 0) {
            out = mPending[--mPendingCount];
            return true;
        }
        return false;
    }
    void pushTouch(TouchAction act, int x, int y) {
        if (mPendingCount < 8) {
            mPending[mPendingCount].type = EventType::TOUCH;
            mPending[mPendingCount].touch.action = act;
            mPending[mPendingCount].touch.x = (int16_t)x;
            mPending[mPendingCount].touch.y = (int16_t)y;
            mPendingCount++;
        }
    }
    Event mPending[8];
    int   mPendingCount = 0;
};

// ── color views (same as demo) ──────────────────────────────────────

static const int kCols = 5, kRows = 5;
static const int kCellW = 128, kCellH = 96;

class ColorView : public View {
public:
    explicit ColorView(RGB565 color) : mColor(color) {}
    void onDraw(Painter& p) override {
        p.fillRect(0, 0, mBounds.width, mBounds.height, mColor);
    }
private:
    RGB565 mColor;
};

// ── IconGrid ────────────────────────────────────────────────────────

class IconGrid : public ViewGroup {
public:
    void setOnIconClick(void (*cb)(void*, int), void* user) { mCb = cb; mUser = user; }
    bool onTouchEvent(TouchEvent& ev) override {
        if (ev.action == TouchAction::DOWN) return true;
        if (ev.action == TouchAction::UP && mCb) {
            int col = ev.x / kCellW, row = ev.y / kCellH;
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

// ── GalleryActivity ─────────────────────────────────────────────────

class GalleryActivity : public Activity {
public:
    void onCreate(Bundle& state) override {
        (void)state;
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
            int col = i % kCols, row = i / kCols;
            int cx = col * kCellW + 14, cy = row * kCellH;
            auto* icon = new ImageView(100, 100);
            icon->bounds() = {(int16_t)cx, (int16_t)cy, 100, 100};
            icon->setImageId((ImageId)i);
            root->addView(icon);
        }
    }
};

// ── DetailActivity (feature showcase) ───────────────────────────────

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

        int bw = 100, bh = 100, y1 = 60, y2 = 220;

        addImage(root, 50,  y1, bw, bh, (ImageId)mIndex);       // [0] original
        addImage(root, 210, y1, bw, bh, IMG_G_MUSIC)            // [1] grayscale+tint
            ->setTintColor(RGB565::fromRGB(255, 80, 80));
        addImage(root, 370, y1, bw, bh, (ImageId)mIndex)        // [2] 90° rot
            ->setRotationAngle(90);
        addImage(root, 50,  y2, bw, bh, (ImageId)mIndex)        // [3] gold tint
            ->setTintColor(RGB565::fromRGB(255, 200, 60));
        addImage(root, 210, y2, bw, bh, (ImageId)mIndex);       // [4] animated

        addImage(root, 370, y2, bw, bh, IMG_G_MUSIC)            // [5] 180° + tint
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
    }

private:
    ImageView* addImage(ViewGroup* parent, int x, int y, int w, int h,
                        ImageId id) {
        auto* v = new ImageView(w, h);
        v->bounds() = {(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h};
        v->setImageId(id);
        parent->addView(v);
        for (int i = 0; i < 6; i++)
            if (!mImages[i]) { mImages[i] = v; break; }
        return v;
    }
    int        mIndex = 0;
    ImageView* mImages[6] = {};
};

// ── PPM writer (RGB565 → RGB888 text PPM) ──────────────────────────

static void writePPM(const char* path, const uint16_t* buf, int w, int h) {
    FILE* f = fopen(path, "wb");
    if (!f) { printf("Cannot open %s\n", path); return; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        uint16_t v = buf[i];
        uint8_t r = (uint8_t)(((v >> 11) & 0x1F) * 255 / 31);
        uint8_t g = (uint8_t)(((v >> 5)  & 0x3F) * 255 / 63);
        uint8_t b = (uint8_t)(( v        & 0x1F) * 255 / 31);
        fwrite(&r, 1, 1, f);
        fwrite(&g, 1, 1, f);
        fwrite(&b, 1, 1, f);
    }
    fclose(f);
    printf("Wrote %s (%dx%d)\n", path, w, h);
}

// ── main ────────────────────────────────────────────────────────────

int main() {
    MemDisplay display;
    display.init(640, 480);
    MemInput input;
    MemTick tick;

    WindowManager wm(display, input, tick);
    wm.initPFB(128, 4, 2);

    ActivityManager am(wm);
    am.registerActivity<GalleryActivity>("GalleryActivity");
    am.registerActivity<DetailActivity>("DetailActivity");

    // Start on detail page with index 0
    Intent intent;
    intent.target = "DetailActivity";
    intent.putInt("index", 0);
    am.startActivity(intent);

    // Render one frame
    wm.runOnce();

    // Save
    writePPM("/tmp/litho_detail_0.ppm", gFramebuffer, 640, 480);

    // Now navigate: click on icon index 5 in gallery
    intent.target = "GalleryActivity";
    am.startActivity(intent);
    wm.runOnce();
    writePPM("/tmp/litho_gallery.ppm", gFramebuffer, 640, 480);

    // Detail page with index 5
    intent.target = "DetailActivity";
    intent.putInt("index", 5);
    am.startActivity(intent);
    wm.runOnce();
    writePPM("/tmp/litho_detail_5.ppm", gFramebuffer, 640, 480);

    delete[] gFramebuffer;
    return 0;
}
