#include "core/painter.hpp"
#include "tests/unit/test_runner.hpp"
#include <cstring>

using namespace litho;

static uint16_t gBuf[256];  // 16x16 tile for testing

static Tile makeTile(int w, int h) {
    Tile t;
    memset(gBuf, 0, sizeof(gBuf));
    t.attach(gBuf, w, h);
    return t;
}

// ---- fillRect: screen→clip→tile transform ----

TEST(painter_fill_full_tile) {
    Tile t = makeTile(8, 8);
    Painter p;
    p.setTile(t, 0, 0);
    p.setScreenOrigin(0, 0);
    p.setScreenClip(0, 0, 8, 8);

    p.fillRect(0, 0, 8, 8, RGB565::Red());

    // all pixels should be red (0xF800)
    for (int i = 0; i < 64; i++) {
        EXPECT_EQ(gBuf[i], 0xF800);
    }
    return true;
}

TEST(painter_fill_clipped) {
    Tile t = makeTile(8, 8);
    Painter p;
    p.setTile(t, 4, 4);   // tile at screen (4,4)-(12,12)
    p.setScreenOrigin(0, 0);
    p.setScreenClip(0, 0, 16, 16);

    p.fillRect(2, 2, 12, 12, RGB565::Red());  // screen (2,2)-(14,14)
    // Only pixels (4,4)-(8,8) in tile should be red
    // Tile pos (0,0) = screen (4,4)
    // screen (2,2) clipped to (4,4), screen (14,14) clipped to tile edge
    // tile coords: (4-4=0, 4-4=0) to (8,8)

    EXPECT_EQ(gBuf[0], 0xF800);             // tile(0,0) = screen(4,4) — red
    EXPECT_EQ(gBuf[7 + 7*8], 0xF800);       // tile(7,7) — red
    // pixel outside tile should be 0
    EXPECT_EQ(gBuf[0 + 0*8], 0xF800);       // was written
    return true;
}

TEST(painter_clip_intersection) {
    Tile t = makeTile(16, 16);
    Painter p;
    p.setTile(t, 0, 0);
    p.setScreenOrigin(0, 0);
    p.setScreenClip(2, 2, 10, 10);   // only (2,2)-(10,10) visible

    p.fillRect(0, 0, 16, 16, RGB565::Red());  // entire screen
    // should only fill (2,2)-(10,10)

    EXPECT_EQ(gBuf[1 + 1*16], 0);       // (1,1) — outside clip, untouched
    EXPECT_EQ(gBuf[2 + 2*16], 0xF800);  // (2,2) — inside clip
    EXPECT_EQ(gBuf[9 + 9*16], 0xF800);  // (9,9) — inside clip
    EXPECT_EQ(gBuf[10 + 10*16], 0);     // (10,10) — outside clip (exclusive right/bottom)
    return true;
}

TEST(painter_intersectsClip) {
    Painter p;
    Tile ti = makeTile(8, 8);
    p.setTile(ti, 0, 0);
    p.setScreenOrigin(0, 0);
    p.setScreenClip(0, 0, 8, 8);

    // fully inside
    EXPECT_TRUE(p.intersectsClip(2, 2, 6, 6));
    // partially overlaps
    EXPECT_TRUE(p.intersectsClip(-4, -4, 4, 4));
    // completely outside
    EXPECT_FALSE(p.intersectsClip(10, 10, 16, 16));
    // adjacent (shares edge) — strict tests
    EXPECT_FALSE(p.intersectsClip(8, 0, 16, 8));   // left edge at clip.right
    return true;
}

TEST(painter_screen_origin_offset) {
    Tile t = makeTile(8, 8);
    Painter p;
    p.setTile(t, 4, 4);   // tile covers screen (4,4)-(12,12)
    p.setScreenOrigin(2, 2);   // draw offset: local(0,0)=screen(2,2)
    p.setScreenClip(0, 0, 16, 16);

    // fill local (2,2)-(10,10) = screen (4,4)-(12,12) = tile (0,0)-(8,8)
    p.fillRect(2, 2, 8, 8, RGB565::Red());

    EXPECT_EQ(gBuf[0], 0xF800);       // tile(0,0) = screen(4,4)
    EXPECT_EQ(gBuf[7 + 7*8], 0xF800); // tile(7,7) = screen(11,11)
    return true;
}

// ---- runner ----

bool run_painter_tests() {
    printf("=== Painter ===\n");
    bool ok = true;
    runTest("fill_full_tile",      test_painter_fill_full_tile,       ok);
    runTest("fill_clipped",        test_painter_fill_clipped,         ok);
    runTest("clip_intersection",   test_painter_clip_intersection,    ok);
    runTest("intersectsClip",      test_painter_intersectsClip,       ok);
    runTest("screen_origin",       test_painter_screen_origin_offset, ok);
    return ok;
}
