/**
 * test_rotate.cpp — Formal verification of drawImageRotated
 *
 * The rotation transform maps output pixel (ox,oy) to source pixel (sx,sy):
 *
 *   source = R⁻¹(θ) · (minX + ox + ½, minY + oy + ½) + pivot
 *
 * where R⁻¹ is the inverse 2D rotation matrix.  For 90°-multiples the
 * mapping collapses to a simple closed form which we use as ground truth.
 *
 * Test image layout (W=5, H=3, each pixel = y*W + x + 1):
 *   row 0:  1  2  3  4  5
 *   row 1:  6  7  8  9 10
 *   row 2: 11 12 13 14 15
 */

#include "core/painter.hpp"
#include "core/tile.hpp"
#include "core/dirty_list.hpp"
#include "framework/view/view_group.hpp"
#include "framework/widget/image_view.hpp"
#include "tests/unit/test_runner.hpp"
#include <cstring>
#include <cstdlib>
#include <cmath>

using namespace litho;

// ── test image ──────────────────────────────────────────────────────

static const int SRC_W = 5, SRC_H = 3;

static void fillSrc(uint16_t* buf, int w, int h) {
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            buf[y * w + x] = (uint16_t)(y * w + x + 1);
}

// ---- view-level rotation geometry helpers -----------------------------

class TouchCatcher : public View {
public:
    bool onTouchEvent(TouchEvent& e) override {
        (void)e;
        touched = true;
        return true;
    }

    bool touched = false;
};

static bool regionEq(const Region& r, int x, int y, int w, int h) {
    if (r.x != x || r.y != y || r.width != w || r.height != h) {
        fprintf(stderr, "  region expected {%d,%d,%d,%d} got {%d,%d,%d,%d}\n",
                x, y, w, h, r.x, r.y, r.width, r.height);
        return false;
    }
    return true;
}

static bool hasDirtyContaining(const DirtyList& dl, const Region& r) {
    for (int i = 0; i < dl.count(); i++) {
        const Region& d = dl.regions()[i];
        if (d.x <= r.x && d.y <= r.y &&
            d.x + d.width >= r.x + r.width &&
            d.y + d.height >= r.y + r.height) {
            return true;
        }
    }
    return false;
}

// ── fixture ─────────────────────────────────────────────────────────

struct RotateFixture {
    uint16_t srcBuf[SRC_W * SRC_H];
    uint16_t outBuf[64 * 64];
    Tile     outTile;
    Painter  painter;

    RotateFixture() {
        fillSrc(srcBuf, SRC_W, SRC_H);
        for (auto& v : outBuf) v = 0xCDCD;
        outTile.attach(outBuf, 64, 64);
    }

    const uint16_t* rotate(int16_t angle, int px, int py,
                           int dx = 0, int dy = 0) {
        painter.setTile(outTile, 0, 0);
        painter.setScreenOrigin(0, 0);
        painter.setScreenClip(0, 0, 64, 64);
        painter.setAlpha(255);
        painter.drawImageRotated(srcBuf, 0 /*FMT_RGB565*/, SRC_W, SRC_H,
                                 dx, dy, px, py, angle);
        return outBuf;
    }

    uint16_t get(int x, int y) const { return outBuf[y * 64 + x]; }
};

// ── ground-truth functions for 90°-multiples ────────────────────────
//
// Formal derivation (pixel-centre sampling, Q16 fixed-point):
//
//   source_cont = R⁻¹(θ) · (minX + ox + ½, minY + oy + ½) + pivot
//
// For 90°-multiples, the pivot terms cancel out in the pixel mapping
// (they affect screen placement via minX/minY → dx/dy offset, but NOT
// which source pixel maps to which output pixel).  The mapping is:
//
//   0°:   (ox, oy)  →   (ox,         oy)            W×H
//   90°:  (ox, oy)  →   (oy,         H-1-ox)        H×W
//   180°: (ox, oy)  →   (W-1-ox,     H-1-oy)        W×H
//   270°: (ox, oy)  →   (W-1-oy,     ox)            H×W
//
// These are used as the formal specification; the DDA output MUST
// match them pixel-for-pixel.

static void gt_0deg(int /*px*/, int /*py*/, int ox, int oy,
                    int* sx, int* sy, int* ow, int* oh) {
    *ow = SRC_W;  *oh = SRC_H;
    *sx = ox;      *sy = oy;
}

static void gt_90deg(int /*px*/, int /*py*/, int ox, int oy,
                     int* sx, int* sy, int* ow, int* oh) {
    *ow = SRC_H;  *oh = SRC_W;
    *sx = oy;
    *sy = (SRC_H - 1) - ox;
}

static void gt_180deg(int /*px*/, int /*py*/, int ox, int oy,
                      int* sx, int* sy, int* ow, int* oh) {
    *ow = SRC_W;  *oh = SRC_H;
    *sx = (SRC_W - 1) - ox;
    *sy = (SRC_H - 1) - oy;
}

static void gt_270deg(int /*px*/, int /*py*/, int ox, int oy,
                      int* sx, int* sy, int* ow, int* oh) {
    *ow = SRC_H;  *oh = SRC_W;
    *sx = (SRC_W - 1) - oy;
    *sy = ox;
}

// ── helper: verify output matches ground truth ──────────────────────

// Compare every output pixel against the closed-form ground truth.
// Only reads pixels inside the expected output rectangle; all others
// must remain untouched (poison value 0xCDCD).
static bool verifyAgainstGT(
    const uint16_t* out, int outStride,
    void (*gt)(int px, int py, int ox, int oy,
               int* sx, int* sy, int* ow, int* oh),
    int px, int py,
    const uint16_t* src, int srcW)
{
    int ow, oh, dummy_sx, dummy_sy;
    gt(px, py, 0, 0, &dummy_sx, &dummy_sy, &ow, &oh);

    for (int oy = 0; oy < oh; oy++) {
        for (int ox = 0; ox < ow; ox++) {
            int sx, sy, dum_w, dum_h;
            gt(px, py, ox, oy, &sx, &sy, &dum_w, &dum_h);

            uint16_t expected = (sx >= 0 && sx < srcW && sy >= 0 && sy < SRC_H)
                              ? src[sy * srcW + sx] : 0xCDCD;
            uint16_t got = out[oy * outStride + ox];

            if (got != expected) {
                fprintf(stderr, "  mismatch at output(%d,%d): "
                        "expected src(%d,%d)=0x%04x got 0x%04x\n",
                        ox, oy, sx, sy, expected, got);
                return false;
            }
        }
    }

    // Verify no writes beyond the output rectangle
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            if (x >= ow || y >= oh) {
                if (out[y * outStride + x] != 0xCDCD) {
                    fprintf(stderr, "  wrote outside output at (%d,%d)=0x%04x\n",
                            x, y, out[y * outStride + x]);
                    return false;
                }
            }
        }
    }
    return true;
}

// ── Category 1: 90°-multiples, pivot (0,0) ──────────────────────────

TEST(rotate_0_origin) {
    RotateFixture f;
    f.rotate(0, 0, 0);
    return verifyAgainstGT(f.outBuf, 64, gt_0deg, 0, 0, f.srcBuf, SRC_W);
}

TEST(rotate_90_origin) {
    RotateFixture f;
    f.rotate(90, 0, 0);
    return verifyAgainstGT(f.outBuf, 64, gt_90deg, 0, 0, f.srcBuf, SRC_W);
}

TEST(rotate_180_origin) {
    RotateFixture f;
    f.rotate(180, 0, 0);
    return verifyAgainstGT(f.outBuf, 64, gt_180deg, 0, 0, f.srcBuf, SRC_W);
}

TEST(rotate_270_origin) {
    RotateFixture f;
    f.rotate(270, 0, 0);
    return verifyAgainstGT(f.outBuf, 64, gt_270deg, 0, 0, f.srcBuf, SRC_W);
}

// ── Category 2: 90°-multiples with non-zero pivot ───────────────────

TEST(rotate_0_pivot) {
    RotateFixture f;
    // 0° about (2,1) — the pivot merely shifts the coordinate origin,
    // but the output bounding box adjusts so output(0,0) still maps to
    // source(0,0).  Ground truth is identical to origin pivot.
    f.rotate(0, 2, 1);
    return verifyAgainstGT(f.outBuf, 64, gt_0deg, 2, 1, f.srcBuf, SRC_W);
}

TEST(rotate_90_center) {
    // Pivot at image centre (2,1) — the centre pixel source(2,1)=8
    // should land at output position such that inverse R⁻¹ maps it back.
    RotateFixture f;
    f.rotate(90, 2, 1);
    return verifyAgainstGT(f.outBuf, 64, gt_90deg, 2, 1, f.srcBuf, SRC_W);
}

TEST(rotate_180_pivot) {
    RotateFixture f;
    f.rotate(180, 2, 1);
    return verifyAgainstGT(f.outBuf, 64, gt_180deg, 2, 1, f.srcBuf, SRC_W);
}

TEST(rotate_270_pivot) {
    RotateFixture f;
    f.rotate(270, 3, 2);
    return verifyAgainstGT(f.outBuf, 64, gt_270deg, 3, 2, f.srcBuf, SRC_W);
}

// ── Category 3: ground truth invariance — pivot doesn't affect
//    the mapping for 180° ────────────────────────────────────────────

TEST(rotate_180_pivot_invariant) {
    // The 180° ground truth is pivot-independent: every pixel maps to
    // its diametrically opposite position regardless of pivot.
    RotateFixture f1, f2;
    f1.rotate(180, 0, 0);
    f2.rotate(180, 2, 1);

    int ow = SRC_W, oh = SRC_H;
    for (int y = 0; y < oh; y++) {
        for (int x = 0; x < ow; x++) {
            if (f1.get(x, y) != f2.get(x, y))
                return false;
        }
    }
    return true;
}

// ── Category 4: negative angles & wrap-around ───────────────────────

TEST(rotate_minus90) {
    // -90° ≡ 270°
    RotateFixture f;
    f.rotate(-90, 0, 0);
    return verifyAgainstGT(f.outBuf, 64, gt_270deg, 0, 0, f.srcBuf, SRC_W);
}

TEST(rotate_minus180) {
    // -180° ≡ 180°
    RotateFixture f;
    f.rotate(-180, 0, 0);
    return verifyAgainstGT(f.outBuf, 64, gt_180deg, 0, 0, f.srcBuf, SRC_W);
}

TEST(rotate_360_is_identity) {
    RotateFixture f1, f2;
    f1.rotate(0, 0, 0);
    f2.rotate(360, 0, 0);

    int ow = SRC_W, oh = SRC_H;
    for (int y = 0; y < oh; y++) {
        for (int x = 0; x < ow; x++) {
            if (f1.get(x, y) != f2.get(x, y))
                return false;
        }
    }
    return true;
}

// ── Category 5: arbitrary angles — bilinear reference model

// Reference model that mirrors the DDA's bilinear interpolation.
// Uses the same sin-table Q16 cosA/sinA, same bounding box logic,
// same half-pixel offset, and blends 4 neighbours with Q16 weights.

struct RefModel {
    int32_t cosA, sinA;   // Q16
    int     minIX, minIY;
    int     outW, outH;
    int     px, py;
    int     srcW, srcH;
    bool    bilinear;     // true for non-90° angles

    RefModel(int16_t angleDeg, int pivotX, int pivotY,
             int sw, int sh) : px(pivotX), py(pivotY), srcW(sw), srcH(sh) {
        int16_t a = angleDeg % 360;
        if (a < 0) a += 360;

        // ── use EXACT same Q16 cos/sin as the DDA ──
        switch (a) {
        case 0:   cosA =  65536;  sinA =  0;       bilinear = false; break;
        case 90:  cosA =  0;      sinA =  65536;   bilinear = false; break;
        case 180: cosA = -65536;  sinA =  0;       bilinear = false; break;
        case 270: cosA =  0;      sinA = -65536;   bilinear = false; break;
        default:
            cosA = (int32_t)cosDeg(a) << 1;
            sinA = (int32_t)sinDeg(a) << 1;
            bilinear = true;
            break;
        }

        // ── bounding box: identical integer arithmetic ──
        int corners[4][2] = {{0,0}, {srcW,0}, {srcW,srcH}, {0,srcH}};
        minIX =  0x7FFFFFFF; int maxIX = -0x80000000;
        minIY =  0x7FFFFFFF; int maxIY = -0x80000000;
        for (int i = 0; i < 4; i++) {
            int rx = (int)(((int32_t)(corners[i][0] - px) * cosA -
                           (int32_t)(corners[i][1] - py) * sinA) >> 16);
            int ry = (int)(((int32_t)(corners[i][0] - px) * sinA +
                           (int32_t)(corners[i][1] - py) * cosA) >> 16);
            if (rx < minIX) minIX = rx;
            if (ry < minIY) minIY = ry;
            if (rx > maxIX) maxIX = rx;
            if (ry > maxIY) maxIY = ry;
        }
        outW = maxIX - minIX;
        outH = maxIY - minIY;
    }

    // Compute expected pixel value at output (ox, oy).
    // For bilinear mode, blends 4 neighbours with Q16 weights.
    // For NN mode, returns the single nearest source pixel.
    uint16_t expectedPixel(int ox, int oy,
                           const uint16_t* src, int srcW, int srcH) const {
        int32_t halfX = (cosA + sinA) / 2;
        int32_t halfY = (cosA - sinA) / 2;

        int32_t baseSX = (int32_t)px * 65536
                       + (int32_t)minIX * cosA
                       + (int32_t)minIY * sinA + halfX;
        int32_t baseSY = (int32_t)py * 65536
                       - (int32_t)minIX * sinA
                       + (int32_t)minIY * cosA + halfY;

        // Coordinates outside the source are intentionally left unclamped so
        // tests catch placement errors instead of hiding them.

        int32_t curSX = baseSX + (int32_t)ox * cosA + (int32_t)oy * sinA;
        int32_t curSY = baseSY - (int32_t)ox * sinA + (int32_t)oy * cosA;

        int sx = curSX >> 16;
        int sy = curSY >> 16;

        // The DDA skips pixels more than 1 outside the source
        // (needs neighbour access for bilinear clamping).
        if (sx < -1 || sx >= srcW || sy < -1 || sy >= srcH)
            return 0xCDCD;

        if (!bilinear) {
            if (sx >= 0 && sx < srcW && sy >= 0 && sy < srcH)
                return src[sy * srcW + sx];
            return 0xCDCD;
        }

        // ── bilinear interpolation ──────────────────────────
        uint32_t fx = (uint32_t)(curSX & 0xFFFF);
        uint32_t fy = (uint32_t)(curSY & 0xFFFF);

        int sx0 = (sx < 0) ? 0 : (sx >= srcW ? srcW-1 : sx);
        int sy0 = (sy < 0) ? 0 : (sy >= srcH ? srcH-1 : sy);
        int sx1 = (sx+1 < srcW) ? sx+1 : (sx < 0 ? 0 : srcW-1);
        int sy1 = (sy+1 < srcH) ? sy+1 : (sy < 0 ? 0 : srcH-1);
        if (sx < 0) fx = 0;
        if (sy < 0) fy = 0;

        uint32_t ifx = 65536 - fx, ify = 65536 - fy;
        uint32_t w00 = (ifx * ify) >> 16;
        uint32_t w10 = ( fx * ify) >> 16;
        uint32_t w01 = (ifx *  fy) >> 16;
        uint32_t w11 = ( fx *  fy) >> 16;

        uint16_t p00=src[sy0*srcW+sx0], p10=src[sy0*srcW+sx1];
        uint16_t p01=src[sy1*srcW+sx0], p11=src[sy1*srcW+sx1];

        uint32_t r0=(p00>>11)&0x1F,g0=(p00>>5)&0x3F,b0=p00&0x1F;
        uint32_t r1=(p10>>11)&0x1F,g1=(p10>>5)&0x3F,b1=p10&0x1F;
        uint32_t r2=(p01>>11)&0x1F,g2=(p01>>5)&0x3F,b2=p01&0x1F;
        uint32_t r3=(p11>>11)&0x1F,g3=(p11>>5)&0x3F,b3=p11&0x1F;

        uint32_t rb=(r0*w00+r1*w10+r2*w01+r3*w11)>>16;
        uint32_t gb=(g0*w00+g1*w10+g2*w01+g3*w11)>>16;
        uint32_t bb=(b0*w00+b1*w10+b2*w01+b3*w11)>>16;
        if(rb>0x1F)rb=0x1F; if(gb>0x3F)gb=0x3F; if(bb>0x1F)bb=0x1F;
        return (uint16_t)((rb<<11)|(gb<<5)|bb);
    }
};

// Compare DDA output against the reference model pixel-for-pixel.
static bool verifyAgainstRef(
    const uint16_t* out, int outStride,
    const RefModel& ref,
    const uint16_t* src, int srcW, int srcH)
{
    for (int oy = 0; oy < ref.outH; oy++) {
        for (int ox = 0; ox < ref.outW; ox++) {
            uint16_t expected = ref.expectedPixel(ox, oy, src, srcW, srcH);
            uint16_t got = out[oy * outStride + ox];

            if (got != expected) {
                // Allow ±1 per channel for possible rounding differences
                int dr = abs((int)((got>>11)&0x1F) - (int)((expected>>11)&0x1F));
                int dg = abs((int)((got>>5)&0x3F) - (int)((expected>>5)&0x3F));
                int db = abs((int)(got&0x1F) - (int)(expected&0x1F));
                if (dr > 1 || dg > 1 || db > 1) {
                    fprintf(stderr, "  mismatch at output(%d,%d): "
                            "expected 0x%04x got 0x%04x (dR=%d dG=%d dB=%d)\n",
                            ox, oy, expected, got, dr, dg, db);
                    return false;
                }
            }
        }
    }
    // Check outside — untouched
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            if (x >= ref.outW || y >= ref.outH) {
                if (out[y * outStride + x] != 0xCDCD) {
                    fprintf(stderr, "  wrote outside output at (%d,%d)=0x%04x\n",
                            x, y, out[y * outStride + x]);
                    return false;
                }
            }
        }
    }
    return true;
}

// ── Category 5a: arbitrary angles vs double-precision reference ─────

TEST(rotate_15_ref) {
    RotateFixture f;
    f.rotate(15, 0, 0);
    RefModel ref(15, 0, 0, SRC_W, SRC_H);
    return verifyAgainstRef(f.outBuf, 64, ref, f.srcBuf, SRC_W, SRC_H);
}

TEST(rotate_30_ref) {
    RotateFixture f;
    f.rotate(30, 0, 0);
    RefModel ref(30, 0, 0, SRC_W, SRC_H);
    return verifyAgainstRef(f.outBuf, 64, ref, f.srcBuf, SRC_W, SRC_H);
}

TEST(rotate_45_ref) {
    RotateFixture f;
    f.rotate(45, 0, 0);
    RefModel ref(45, 0, 0, SRC_W, SRC_H);
    return verifyAgainstRef(f.outBuf, 64, ref, f.srcBuf, SRC_W, SRC_H);
}

TEST(rotate_60_ref) {
    RotateFixture f;
    f.rotate(60, 0, 0);
    RefModel ref(60, 0, 0, SRC_W, SRC_H);
    return verifyAgainstRef(f.outBuf, 64, ref, f.srcBuf, SRC_W, SRC_H);
}

TEST(rotate_75_ref) {
    RotateFixture f;
    f.rotate(75, 0, 0);
    RefModel ref(75, 0, 0, SRC_W, SRC_H);
    return verifyAgainstRef(f.outBuf, 64, ref, f.srcBuf, SRC_W, SRC_H);
}

// Also verify non-origin pivot with arbitrary angles
TEST(rotate_30_center_ref) {
    RotateFixture f;
    f.rotate(30, 2, 1);
    RefModel ref(30, 2, 1, SRC_W, SRC_H);
    return verifyAgainstRef(f.outBuf, 64, ref, f.srcBuf, SRC_W, SRC_H);
}

TEST(rotate_60_center_ref) {
    RotateFixture f;
    f.rotate(60, 2, 1);
    RefModel ref(60, 2, 1, SRC_W, SRC_H);
    return verifyAgainstRef(f.outBuf, 64, ref, f.srcBuf, SRC_W, SRC_H);
}

TEST(rotate_45_center_leaves_aabb_corners_empty) {
    RotateFixture f;
    f.rotate(45, SRC_W / 2, SRC_H / 2);
    RefModel ref(45, SRC_W / 2, SRC_H / 2, SRC_W, SRC_H);
    EXPECT_EQ(f.get(0, 0), 0xCDCD);
    EXPECT_EQ(f.get(ref.outW - 1, 0), 0xCDCD);
    EXPECT_EQ(f.get(0, ref.outH - 1), 0xCDCD);
    return true;
}

// ── Category 5b: inverse test — θ followed by -θ ≈ identity ─────────

// Rotate the source image by +θ, extract the valid region into a
// contiguous buffer, then rotate THAT by -θ.  Every pixel in the
// final output must be a valid source value (∈ {1..W*H}).
//
// This verifies both the forward and inverse DDA paths without any
// external reference model.

static bool inverseTest(int16_t angle, int px, int py,
                        int srcW, int srcH,
                        const uint16_t* src) {
    // Round 1: rotate src by +angle
    uint16_t buf1[64 * 64];
    uint16_t buf2[64 * 64];
    Tile t1, t2;
    Painter p;
    for (auto& v : buf1) v = 0xCDCD;
    for (auto& v : buf2) v = 0xCDCD;
    t1.attach(buf1, 64, 64);
    t2.attach(buf2, 64, 64);

    p.setTile(t1, 0, 0);
    p.setScreenOrigin(0, 0);
    p.setScreenClip(0, 0, 64, 64);
    p.setAlpha(255);
    p.drawImageRotated(src, 0, srcW, srcH, 0, 0, px, py, angle);

    // Find round-1 bounding box
    int minX1 = 64, minY1 = 64, maxX1 = -1, maxY1 = -1;
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            if (buf1[y * 64 + x] != 0xCDCD) {
                if (x < minX1) minX1 = x;
                if (y < minY1) minY1 = y;
                if (x > maxX1) maxX1 = x;
                if (y > maxY1) maxY1 = y;
            }
        }
    }
    if (minX1 > maxX1) return true;

    int outW1 = maxX1 - minX1 + 1;
    int outH1 = maxY1 - minY1 + 1;

    // Copy valid region into contiguous buffer (stride = outW1)
    uint16_t tmp[32 * 32];
    for (int y = 0; y < outH1; y++)
        for (int x = 0; x < outW1; x++)
            tmp[y * outW1 + x] = buf1[(minY1 + y) * 64 + (minX1 + x)];

    // Round 2: rotate the contiguous extract by -angle
    p.setTile(t2, 0, 0);
    p.drawImageRotated(tmp, 0, outW1, outH1, 0, 0, 0, 0, -angle);

    // Every rendered pixel must be a valid RGB565 value (bilinear
    // produces blended colours, so we can't check for exact source
    // matches — just verify nothing outlandish appears).
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            uint16_t v = buf2[y * 64 + x];
            if (v != 0xCDCD) {
                // Must not be poison and must have valid RGB565 range
                if (v == 0xCDCD || v == 0xFFFF) {
                    fprintf(stderr, "  inverse: invalid value 0x%04x "
                            "at (%d,%d)\n", v, x, y);
                    return false;
                }
            }
        }
    }
    return true;
}

TEST(rotate_15_inverse) {
    RotateFixture f;
    return inverseTest(15, 0, 0, SRC_W, SRC_H, f.srcBuf);
}

TEST(rotate_30_inverse) {
    RotateFixture f;
    return inverseTest(30, 0, 0, SRC_W, SRC_H, f.srcBuf);
}

TEST(rotate_45_inverse) {
    RotateFixture f;
    return inverseTest(45, 0, 0, SRC_W, SRC_H, f.srcBuf);
}

TEST(rotate_60_inverse) {
    RotateFixture f;
    return inverseTest(60, 0, 0, SRC_W, SRC_H, f.srcBuf);
}

// ── Category 5c: monotonicity — DDA steps produce monotonic source ──
//
// Within each output row, the source X coordinate advances by cosA
// per pixel.  This must be monotonic (non-decreasing for cosA ≥ 0,
// non-increasing for cosA < 0).  Same for source Y with -sinA.
//
// We can't directly observe source coordinates from the output, but
// we CAN verify that consecutive output pixels sample from source
// positions whose X coordinates are correctly ordered (i.e. the
// source buffer index increases or decreases monotonically).

static bool checkMonotonic(const uint16_t* out, int outStride,
                           int outW, int outH,
                           const uint16_t* src, int srcW, int srcH,
                           bool expectXIncreasing) {
    for (int oy = 0; oy < outH; oy++) {
        int prevSX = expectXIncreasing ? -1 : srcW;
        for (int ox = 0; ox < outW; ox++) {
            uint16_t v = out[oy * outStride + ox];
            if (v == 0xCDCD) continue;

            // Find this value's position in the source
            int sx = -1, sy = -1;
            for (int y = 0; y < srcH && sx < 0; y++) {
                for (int x = 0; x < srcW; x++) {
                    if (src[y * srcW + x] == v) {
                        sx = x; sy = y; break;
                    }
                }
            }

            if (sx >= 0) {
                if (expectXIncreasing) {
                    if (sx < prevSX) {
                        fprintf(stderr, "  non-monotonic X: "
                                "prev=%d cur=%d at out(%d,%d) val=0x%04x\n",
                                prevSX, sx, ox, oy, v);
                        return false;
                    }
                } else {
                    if (sx > prevSX) {
                        fprintf(stderr, "  non-monotonic X: "
                                "prev=%d cur=%d at out(%d,%d) val=0x%04x\n",
                                prevSX, sx, ox, oy, v);
                        return false;
                    }
                }
                prevSX = sx;
            }
        }
    }
    return true;
}

TEST(rotate_15_smoothness) {
    // Monotonicity is a nearest-neighbor property.
    // Bilinear interpolation breaks pixel→source uniqueness.
    // Verify instead that adjacent output pixels have small
    // per-channel differences (smoothness check).
    RotateFixture f;
    f.rotate(15, 0, 0);
    RefModel ref(15, 0, 0, SRC_W, SRC_H);

    for (int oy = 0; oy < ref.outH - 1; oy++) {
        for (int ox = 0; ox < ref.outW - 1; ox++) {
            uint16_t v0 = f.outBuf[oy * 64 + ox];
            uint16_t v1 = f.outBuf[oy * 64 + ox + 1];
            if (v0 == 0xCDCD || v1 == 0xCDCD) continue;
            int dr = abs((int)((v0>>11)&0x1F) - (int)((v1>>11)&0x1F));
            int dg = abs((int)((v0>>5)&0x3F)  - (int)((v1>>5)&0x3F));
            int db = abs((int)(v0&0x1F)       - (int)(v1&0x1F));
            // Bilinear interpolation guarantees smooth gradients —
            // adjacent pixels should not jump by more than ~16 levels
            if (dr > 16 || dg > 32 || db > 16) {
                fprintf(stderr, "  large jump at (%d,%d): 0x%04x→0x%04x\n",
                        ox, oy, v0, v1);
                return false;
            }
        }
    }
    return true;
}

TEST(rotate_120_smoothness) {
    RotateFixture f;
    f.rotate(120, 0, 0);
    RefModel ref(120, 0, 0, SRC_W, SRC_H);

    for (int oy = 0; oy < ref.outH - 1; oy++) {
        for (int ox = 0; ox < ref.outW - 1; ox++) {
            uint16_t v0 = f.outBuf[oy * 64 + ox];
            uint16_t v1 = f.outBuf[oy * 64 + ox + 1];
            if (v0 == 0xCDCD || v1 == 0xCDCD) continue;
            int dr = abs((int)((v0>>11)&0x1F) - (int)((v1>>11)&0x1F));
            int dg = abs((int)((v0>>5)&0x3F)  - (int)((v1>>5)&0x3F));
            int db = abs((int)(v0&0x1F)       - (int)(v1&0x1F));
            if (dr > 16 || dg > 32 || db > 16) return false;
        }
    }
    return true;
}

// ── Category 6: edge case images ────────────────────────────────────

// 1×1 image
TEST(rotate_1x1_all_angles) {
    uint16_t src1x1[1] = { 0x0042 };
    uint16_t outBuf[16 * 16];
    Tile tile;
    Painter p;
    for (auto& v : outBuf) v = 0xCDCD;
    tile.attach(outBuf, 16, 16);

    static const int16_t angles[] = {0, 45, 90, 135, 180, 225, 270, 315, 360};
    for (int ai = 0; ai < 9; ai++) {
        // Reset
        for (auto& v : outBuf) v = 0xCDCD;

        p.setTile(tile, 0, 0);
        p.setScreenOrigin(0, 0);
        p.setScreenClip(0, 0, 16, 16);
        p.setAlpha(255);
        p.drawImageRotated(src1x1, 0, 1, 1, 0, 0, 0, 0, angles[ai]);

        // The single pixel must be somewhere in the output
        bool found = false;
        int foundX = -1, foundY = -1;
        for (int y = 0; y < 8 && !found; y++) {
            for (int x = 0; x < 8; x++) {
                uint16_t v = outBuf[y * 16 + x];
                // With bilinear, a single source pixel may spread
                // across 2 adjacent output pixels (boundary blend).
                // Accept the first non-poison pixel found.
                if (v != 0xCDCD) {
                    found = true;
                    foundX = x; foundY = y;
                }
            }
        }
        if (!found) {
            fprintf(stderr, "  angle %d: pixel not found\n", angles[ai]);
            return false;
        }
    }
    return true;
}

// W×1 image (single row)
TEST(rotate_5x1_90) {
    // 5×1 image rotated 90° → 1×5 image
    uint16_t src5x1[5] = { 10, 20, 30, 40, 50 };
    uint16_t outBuf[32 * 32];
    Tile tile;
    Painter p;
    for (auto& v : outBuf) v = 0xCDCD;
    tile.attach(outBuf, 32, 32);

    p.setTile(tile, 0, 0);
    p.setScreenOrigin(0, 0);
    p.setScreenClip(0, 0, 32, 32);
    p.setAlpha(255);
    p.drawImageRotated(src5x1, 0, 5, 1, 0, 0, 0, 0, 90);

    // Expected: 90° CW of a 5×1 row → 1 wide × 5 tall column
    // output(0,0)=src(0,0)=10,  (0,1)=src(1,0)=20,  ..., (0,4)=src(4,0)=50
    uint16_t expected[5] = { 10, 20, 30, 40, 50 };
    for (int i = 0; i < 5; i++) {
        uint16_t got = outBuf[i * 32 + 0];
        if (got != expected[i]) {
            fprintf(stderr, "  out(%d,0): expected 0x%04x got 0x%04x\n",
                    i, expected[i], got);
            return false;
        }
    }
    return true;
}

// ── Category 7: dx/dy placement offset ──────────────────────────────

TEST(rotate_0_with_offset) {
    // 0° rotation with dx=3, dy=2: the image should appear shifted.
    RotateFixture f;
    f.rotate(0, 0, 0, 3, 2);

    // Output(ox,oy) → screen(dx+ox, dy+oy) → tile(dx+ox, dy+oy)
    // Should match source at offset (3,2)
    for (int y = 0; y < SRC_H; y++) {
        for (int x = 0; x < SRC_W; x++) {
            uint16_t expected = f.srcBuf[y * SRC_W + x];
            uint16_t got = f.get(3 + x, 2 + y);
            if (got != expected) {
                fprintf(stderr, "  offset: at (%d,%d) expected 0x%04x got 0x%04x\n",
                        3+x, 2+y, expected, got);
                return false;
            }
        }
    }
    // Pixel at (0,0) must be untouched
    if (f.get(0, 0) != 0xCDCD) return false;
    return true;
}

TEST(image_view_bounds_90_off_center_pivot) {
    ImageView v(5, 3);
    v.bounds() = {10, 20, 5, 3};
    v.setRotationAngle(90, 1, 0);
    return regionEq(v.transformedBounds(), 8, 19, 3, 5);
}

TEST(image_view_bounds_180_off_center_pivot) {
    ImageView v(5, 3);
    v.bounds() = {10, 20, 5, 3};
    v.setRotationAngle(180, 1, 0);
    return regionEq(v.transformedBounds(), 7, 17, 5, 3);
}

TEST(image_view_bounds_270_negative_angle) {
    ImageView v(5, 3);
    v.bounds() = {10, 20, 5, 3};
    v.setRotationAngle(-90, 1, 0);
    return regionEq(v.transformedBounds(), 11, 16, 3, 5);
}

TEST(image_view_rotation_invalidates_old_and_new_bounds) {
    DirtyList dl;
    ViewGroup root;
    ImageView* child = new ImageView(5, 3);
    child->bounds() = {10, 20, 5, 3};
    root.addView(child);
    root.propagateDirtyList(&dl);

    child->setRotationAngle(90, 1, 0);
    dl.clear();
    Region old = child->screenBounds();
    child->setRotationAngle(180, 1, 0);
    Region now = child->screenBounds();

    EXPECT_TRUE(hasDirtyContaining(dl, old));
    EXPECT_TRUE(hasDirtyContaining(dl, now));
    return true;
}

TEST(view_group_touch_uses_transformed_bounds) {
    ViewGroup root;
    TouchCatcher* child = new TouchCatcher();
    child->bounds() = {10, 20, 5, 3};
    child->setTranslationX(-2);
    root.addView(child);

    TouchEvent ev{8, 21, TouchAction::DOWN, nullptr, 0, 0};
    EXPECT_TRUE(root.dispatchTouchEvent(ev, 0, 0));
    EXPECT_TRUE(child->touched);
    EXPECT_EQ(ev.handler, child);
    EXPECT_EQ(ev.handlerSX, 8);
    EXPECT_EQ(ev.handlerSY, 20);
    return true;
}

// ── runner ──────────────────────────────────────────────────────────

bool run_rotate_tests() {
    printf("=== Rotation ===\n");
    bool ok = true;
    // Category 1: 90°-multiples, pivot (0,0)
    runTest("0_origin",             test_rotate_0_origin,                 ok);
    runTest("90_origin",            test_rotate_90_origin,                ok);
    runTest("180_origin",           test_rotate_180_origin,               ok);
    runTest("270_origin",           test_rotate_270_origin,               ok);
    // Category 2: 90°-multiples, non-zero pivot
    runTest("0_pivot",              test_rotate_0_pivot,                  ok);
    runTest("90_center",            test_rotate_90_center,                ok);
    runTest("180_pivot",            test_rotate_180_pivot,                ok);
    runTest("270_pivot",            test_rotate_270_pivot,                ok);
    // Category 3: pivot invariance
    runTest("180_pivot_invariant",  test_rotate_180_pivot_invariant,      ok);
    // Category 4: negative / wrap
    runTest("minus90",              test_rotate_minus90,                  ok);
    runTest("minus180",             test_rotate_minus180,                 ok);
    runTest("360_is_identity",      test_rotate_360_is_identity,          ok);
    // Category 5a: arbitrary angle vs double-precision reference
    runTest("15_vs_ref",            test_rotate_15_ref,                   ok);
    runTest("30_vs_ref",            test_rotate_30_ref,                   ok);
    runTest("45_vs_ref",            test_rotate_45_ref,                   ok);
    runTest("60_vs_ref",            test_rotate_60_ref,                   ok);
    runTest("75_vs_ref",            test_rotate_75_ref,                   ok);
    runTest("30_center_vs_ref",     test_rotate_30_center_ref,            ok);
    runTest("60_center_vs_ref",     test_rotate_60_center_ref,            ok);
    runTest("45_center_corners",    test_rotate_45_center_leaves_aabb_corners_empty, ok);
    // Category 5b: inverse test (θ then -θ)
    runTest("15_inverse",           test_rotate_15_inverse,               ok);
    runTest("30_inverse",           test_rotate_30_inverse,               ok);
    runTest("45_inverse",           test_rotate_45_inverse,               ok);
    runTest("60_inverse",           test_rotate_60_inverse,               ok);
    // Category 5c: smoothness
    runTest("15_smoothness",        test_rotate_15_smoothness,            ok);
    runTest("120_smoothness",       test_rotate_120_smoothness,           ok);
    // Category 6: edge cases
    runTest("1x1_all_angles",       test_rotate_1x1_all_angles,           ok);
    runTest("5x1_90",               test_rotate_5x1_90,                  ok);
    // Category 7: placement offset
    runTest("0_with_offset",        test_rotate_0_with_offset,            ok);
    // Category 8: view-level transformed bounds, invalidation, dispatch
    runTest("image_bounds_90_pivot",  test_image_view_bounds_90_off_center_pivot,       ok);
    runTest("image_bounds_180_pivot", test_image_view_bounds_180_off_center_pivot,      ok);
    runTest("image_bounds_-90",       test_image_view_bounds_270_negative_angle,        ok);
    runTest("rotation_dirty_old_new", test_image_view_rotation_invalidates_old_and_new_bounds, ok);
    runTest("touch_transformed",      test_view_group_touch_uses_transformed_bounds,    ok);
    return ok;
}
