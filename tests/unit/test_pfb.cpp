#include "core/pfb.hpp"
#include "tests/unit/test_runner.hpp"

using namespace litho;

// Minimal mock display for testing PFB
struct MockDisplay {
    int bitbltCount = 0;
    int totalW = 0, totalH = 0;

    void bitblt(const uint16_t*, int x, int y, int w, int h) {
        bitbltCount++;
        totalW += w;
        totalH += h;
    }
    int width()  const { return 640; }
    int height() const { return 480; }
};

// ---- init ----

TEST(pfb_init) {
    PFB pfb;
    pfb.init(128, 4, 4, 640, 480);
    // 640/128=5 cols, 480/4=120 rows = 600 blocks
    EXPECT_EQ(pfb.cols(), 5);
    EXPECT_EQ(pfb.rows(), 120);
    EXPECT_EQ(pfb.blockW(), 128);
    EXPECT_EQ(pfb.blockH(), 4);
    return true;
}

TEST(pfb_non_divisible_screen) {
    PFB pfb;
    pfb.init(100, 7, 2, 640, 480);
    // 640/100 = 7 cols (last col 640-600=40 wide)
    // 480/7   = 69 rows (last row 480-69*7=480-483= -3 → 3)
    EXPECT_EQ(pfb.cols(), 7);
    EXPECT_EQ(pfb.rows(), 69);
    return true;
}

// ---- drawRegion: block coverage ----

TEST(pfb_draw_region_full_screen) {
    PFB pfb;
    pfb.init(128, 4, 4, 640, 480);
    MockDisplay mock;

    pfb.drawRegion({0, 0, 640, 480}, mock,
        [](Painter&, int, int, int, int) {});

    // 5 cols × 120 rows = 600 blocks
    EXPECT_EQ(mock.bitbltCount, 600);
    return true;
}

TEST(pfb_draw_region_small) {
    PFB pfb;
    pfb.init(128, 4, 4, 640, 480);
    MockDisplay mock;

    // 50x30 region at (10,20)
    pfb.drawRegion({10, 20, 50, 30}, mock,
        [](Painter&, int, int, int, int) {});

    // cols: 10/128=0 to (10+50)/128=0 → 1 col
    // rows: 20/4=5 to (20+30)/4=12.5 → rows 5..12 = 8 rows
    // Total: ~8 blocks (might be more due to ceiling math)
    EXPECT_TRUE(mock.bitbltCount >= 8 && mock.bitbltCount <= 16);
    return true;
}

TEST(pfb_edge_block_size) {
    PFB pfb;
    pfb.init(100, 7, 2, 640, 480);

    int gotBx = -1, gotBy = -1, gotBw = -1, gotBh = -1;
    MockDisplay mock;
    pfb.drawRegion({600, 0, 40, 7}, mock,
        [&](Painter&, int bx, int by, int bw, int bh) {
            gotBx = bx; gotBy = by; gotBw = bw; gotBh = bh;
        });

    EXPECT_EQ(mock.bitbltCount, 1);
    EXPECT_EQ(gotBx, 600);
    EXPECT_EQ(gotBy, 0);
    EXPECT_EQ(gotBw, 40);
    EXPECT_EQ(gotBh, 7);
    return true;
}

// ---- runner ----

bool run_pfb_tests() {
    printf("=== PFB ===\n");
    bool ok = true;
    runTest("init",                 test_pfb_init,                  ok);
    runTest("non_divisible",        test_pfb_non_divisible_screen,  ok);
    runTest("draw_full_screen",     test_pfb_draw_region_full_screen, ok);
    runTest("draw_small_region",    test_pfb_draw_region_small,     ok);
    runTest("edge_block_size",      test_pfb_edge_block_size,       ok);
    return ok;
}
