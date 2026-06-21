/**
 * viz_rotate.cpp — Visual rotation test for debugging
 *
 * Build:  g++ -std=c++17 -I.. -I../build/generated \
 *              viz_rotate.cpp -o viz_rotate && ./viz_rotate
 *
 * Renders an 8×4 rectangle with a distinct pattern and rotates it
 * by various angles, printing ASCII-art visualisations.
 */

#include "core/painter.hpp"
#include "core/tile.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>

using namespace litho;

static const int SRC_W = 8, SRC_H = 4;
static const int OUT_SZ = 40;

// ── source pattern: 8×4 rectangle ───────────────────────────────────
//
//   col:  0  1  2  3  4  5  6  7
//   row0: A  B  C  D  E  F  G  H
//   row1: I  J  K  L  M  N  O  P
//   row2: Q  R  S  T  U  V  W  X
//   row3: Y  Z  0  1  2  3  4  5
//
// Each pixel = y*8 + x + 0x41 ('A' = 0x41)
// We render as RGB565 but the upper byte carries the char.

static void fillPattern(uint16_t* buf, int w, int h) {
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            buf[y * w + x] = (uint16_t)(0x4100 + y * w + x);
}

static char pixelToChar(uint16_t v) {
    if (v == 0xCDCD) return '·';
    int idx = (v - 0x4100);
    if (idx >= 0 && idx < 26) return (char)('A' + idx);
    if (idx >= 26 && idx < 36) return (char)('0' + idx - 26);
    if (idx >= 36) return '?';
    return '.';
}

// ── render helper ───────────────────────────────────────────────────

static void renderAndPrint(Painter& p, Tile& tile, uint16_t* outBuf,
                           const uint16_t* src, int srcW, int srcH,
                           int16_t angle, int px, int py,
                           int dx, int dy) {
    // poison fill
    for (int i = 0; i < OUT_SZ * OUT_SZ; i++) outBuf[i] = 0xCDCD;
    tile.attach(outBuf, OUT_SZ, OUT_SZ);

    p.setTile(tile, 0, 0);
    p.setScreenOrigin(0, 0);
    p.setScreenClip(0, 0, OUT_SZ, OUT_SZ);
    p.setAlpha(255);
    p.drawImageRotated(src, 0, srcW, srcH, dx, dy, px, py, angle);

    // Find bounding box
    int minX = OUT_SZ, minY = OUT_SZ, maxX = -1, maxY = -1;
    for (int y = 0; y < OUT_SZ; y++) {
        for (int x = 0; x < OUT_SZ; x++) {
            if (outBuf[y * OUT_SZ + x] != 0xCDCD) {
                if (x < minX) minX = x;
                if (y < minY) minY = y;
                if (x > maxX) maxX = x;
                if (y > maxY) maxY = y;
            }
        }
    }

    // Print header
    printf("\n─── angle=%d° pivot=(%d,%d) dx=%d dy=%d "
           "bbox=(%d,%d)-(%d,%d) [%d×%d] ───\n",
           angle, px, py, dx, dy,
           minX, minY, maxX, maxY,
           maxX - minX + 1, maxY - minY + 1);

    if (minX > maxX) {
        printf("  (empty output)\n");
        return;
    }

    // Print with padding
    int padLeft = 2;
    int padTop  = 1;
    int padRight = 2;
    int padBottom = 1;

    int printX0 = minX - padLeft;  if (printX0 < 0) printX0 = 0;
    int printY0 = minY - padTop;   if (printY0 < 0) printY0 = 0;
    int printX1 = maxX + padRight; if (printX1 >= OUT_SZ) printX1 = OUT_SZ - 1;
    int printY1 = maxY + padBottom;if (printY1 >= OUT_SZ) printY1 = OUT_SZ - 1;

    // Column numbers
    printf("   ");
    for (int x = printX0; x <= printX1; x++) printf("%2d", x % 100);
    printf("\n");

    for (int y = printY0; y <= printY1; y++) {
        printf("%2d ", y);
        for (int x = printX0; x <= printX1; x++) {
            printf(" %c", pixelToChar(outBuf[y * OUT_SZ + x]));
        }
        printf("\n");
    }
}

// ── main ─────────────────────────────────────────────────────────────

int main() {
    uint16_t srcBuf[SRC_W * SRC_H];
    fillPattern(srcBuf, SRC_W, SRC_H);

    uint16_t outBuf[OUT_SZ * OUT_SZ];
    Tile tile;
    Painter painter;

    printf("Source image (%d×%d):\n", SRC_W, SRC_H);
    printf("   ");
    for (int x = 0; x < SRC_W; x++) printf("%2d", x);
    printf("\n");
    for (int y = 0; y < SRC_H; y++) {
        printf("%2d ", y);
        for (int x = 0; x < SRC_W; x++)
            printf(" %c", pixelToChar(srcBuf[y * SRC_W + x]));
        printf("\n");
    }

    // ── 0° — should be identity ──
    renderAndPrint(painter, tile, outBuf, srcBuf, SRC_W, SRC_H,
                   0, 0, 0, 5, 5);

    // ── 90° — should be 4×8, readable rotated ──
    renderAndPrint(painter, tile, outBuf, srcBuf, SRC_W, SRC_H,
                   90, 0, 0, 5, 20);

    // ── 180° — should be 8×4 flipped both ways ──
    renderAndPrint(painter, tile, outBuf, srcBuf, SRC_W, SRC_H,
                   180, 0, 0, 20, 5);

    // ── 270° — should be 4×8 ──
    renderAndPrint(painter, tile, outBuf, srcBuf, SRC_W, SRC_H,
                   270, 0, 0, 20, 20);

    // ── 90° with centre pivot ──
    renderAndPrint(painter, tile, outBuf, srcBuf, SRC_W, SRC_H,
                   90, SRC_W/2, SRC_H/2, 5, 20);

    // ── 45° ──
    renderAndPrint(painter, tile, outBuf, srcBuf, SRC_W, SRC_H,
                   45, 0, 0, 20, 5);

    // ── 30° ──
    renderAndPrint(painter, tile, outBuf, srcBuf, SRC_W, SRC_H,
                   30, 0, 0, 20, 5);

    return 0;
}
