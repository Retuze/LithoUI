/**
 * viz_spin.cpp — record one full rotation of an icon as a frame sequence.
 *
 * Renders IMG_R_TEST (the rotatable asset) through 0°..359° with the pivot
 * pinned to a fixed canvas point — exactly the path ImageView::onDraw takes —
 * and writes each frame as a binary PPM so the rotation can be scrubbed
 * frame-by-frame to locate the flicker.
 *
 * Build (links the already-built litho.lib, which embeds the image bundle
 * + sin table):
 *
 *   clang++ -D_CRT_SECURE_NO_WARNINGS -I. -Ibuild/generated -O0 -g \
 *           -std=gnu++17 -D_DEBUG -D_DLL -D_MT -Xclang --dependent-lib=msvcrtd \
 *           tests/viz_spin.cpp build/litho.lib -o build/viz_spin.exe
 *
 *   ./build/viz_spin.exe            # -> build/spin_frames/frame_000.ppm ...
 */

#include "core/painter.hpp"
#include "core/tile.hpp"
#include "res_images.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

using namespace litho;

static const ImageId ICON     = IMG_R_TEST;  // the only rotatable asset
static const int     CANVAS   = 160;         // > ceil(100*sqrt2) ≈ 142
static const int     STEP_DEG = 1;           // 360 frames = one full turn

// Background the icon is composited over. Flat colour so any per-frame
// jitter of the icon's edges against it is obvious.
static const uint16_t BG = 0x18E3;           // dark slate gray

// Replicate Painter's exact rotated-AABB top-left (minX,minY) so we can pin
// the pivot to the same canvas pixel on every frame.
static void aabbTopLeft(int srcW, int srcH, int pivX, int pivY,
                        int angle, int& minX, int& minY) {
    int32_t cosA, sinA;
    switch (((angle % 360) + 360) % 360) {
    case 0:   cosA = 65536;  sinA = 0;      break;
    case 90:  cosA = 0;      sinA = 65536;  break;
    case 180: cosA = -65536; sinA = 0;      break;
    case 270: cosA = 0;      sinA = -65536; break;
    default:
        cosA = (int32_t)cosDeg(angle) << 1;
        sinA = (int32_t)sinDeg(angle) << 1;
        break;
    }
    int corners[4][2] = {{0,0}, {srcW,0}, {srcW,srcH}, {0,srcH}};
    minX = 0x7FFFFFFF; minY = 0x7FFFFFFF;
    for (int i = 0; i < 4; i++) {
        int rx = ((int32_t)(corners[i][0] - pivX) * cosA -
                  (int32_t)(corners[i][1] - pivY) * sinA) >> 16;
        int ry = ((int32_t)(corners[i][0] - pivX) * sinA +
                  (int32_t)(corners[i][1] - pivY) * cosA) >> 16;
        if (rx < minX) minX = rx;
        if (ry < minY) minY = ry;
    }
}

static void writePPM(const char* path, const uint16_t* buf, int w, int h) {
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        uint16_t v = buf[i];
        uint8_t r = (uint8_t)((v >> 11) & 0x1F);
        uint8_t g = (uint8_t)((v >> 5)  & 0x3F);
        uint8_t b = (uint8_t)( v        & 0x1F);
        uint8_t rgb[3] = {
            (uint8_t)((r << 3) | (r >> 2)),
            (uint8_t)((g << 2) | (g >> 4)),
            (uint8_t)((b << 3) | (b >> 2)),
        };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char** argv) {
    const char* outDir = (argc > 1) ? argv[1] : "build/spin_frames";

    if (!resSinTable()) {
        fprintf(stderr, "ERROR: no sin table in bundle — arbitrary angles "
                        "unavailable. Was rotatable/ packed?\n");
        return 1;
    }

    const ImageEntry* e     = imageEntry(ICON);
    const void*       src   = (const void*)imagePixels(ICON);
    const uint8_t*    alpha = imageAlpha(ICON);
    const int srcW = e->width, srcH = e->height;
    const int pivX = srcW / 2, pivY = srcH / 2;
    const int CX = CANVAS / 2, CY = CANVAS / 2;

    printf("icon=%d  %dx%d  fmt=%d  pivot=(%d,%d)  canvas=%d  step=%d°  filter=bilinear\n",
           (int)ICON, srcW, srcH, e->format, pivX, pivY, CANVAS, STEP_DEG);

    uint16_t* canvas = (uint16_t*)malloc(sizeof(uint16_t) * CANVAS * CANVAS);
    Tile    tile;
    Painter p;

    int frame = 0;
    for (int angle = 0; angle < 360; angle += STEP_DEG, frame++) {
        // clear to background
        for (int i = 0; i < CANVAS * CANVAS; i++) canvas[i] = BG;
        tile.attach(canvas, CANVAS, CANVAS);

        // pin the pivot to canvas centre regardless of angle
        int minX, minY;
        aabbTopLeft(srcW, srcH, pivX, pivY, angle, minX, minY);

        p.setTile(tile, 0, 0);
        p.setScreenOrigin(CX + minX, CY + minY);
        p.setScreenClip(0, 0, CANVAS, CANVAS);
        p.setAlpha(255);
        p.drawImageRotated(src, e->format, srcW, srcH,
                           0, 0, pivX, pivY, (int16_t)angle, alpha, nullptr,
                           nullptr);

        char path[512];
        snprintf(path, sizeof(path), "%s/frame_%03d.ppm", outDir, frame);
        writePPM(path, canvas, CANVAS, CANVAS);
    }

    printf("wrote %d frames to %s/\n", frame, outDir);
    free(canvas);
    return 0;
}
