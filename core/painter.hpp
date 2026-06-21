#pragma once
#include "tile.hpp"
#include "litho_core.h"
#include "res_images.h"
#include <cstring>
#include <cstdio>

namespace litho {

// Image formats — defined in res_images.h, kept in sync:
//   FMT_RGB565    = 0   opaque
//   FMT_RGB565_A8 = 1   RGB + alpha mask
//   FMT_A8        = 2   single-channel alpha (fill=black, tintable)

class Painter {
public:
    void setTile(Tile& tile, int tileOrgX, int tileOrgY) {
        mTile     = &tile;
        mTileOrgX = tileOrgX;
        mTileOrgY = tileOrgY;
        mClipL = -32768; mClipT = -32768;
        mClipR =  32767; mClipB =  32767;
    }

    void setScreenOrigin(int sx, int sy) { mScreenX = sx; mScreenY = sy; }
    int  screenX() const { return mScreenX; }
    int  screenY() const { return mScreenY; }

    void setAlpha(uint8_t a) { mAlpha = a; }
    uint8_t alpha() const { return mAlpha; }

    bool intersectsClip(int left, int top, int right, int bottom) const {
        return left < mClipR && right > mClipL &&
               top  < mClipB && bottom > mClipT;
    }

    void setScreenClip(int left, int top, int right, int bottom) {
        if (left   > mClipL) mClipL = left;
        if (top    > mClipT) mClipT = top;
        if (right  < mClipR) mClipR = right;
        if (bottom < mClipB) mClipB = bottom;
    }

    void fillRect(int x, int y, int w, int h, RGB565 c) {
        int sx0 = x + mScreenX;
        int sy0 = y + mScreenY;
        int sx1 = sx0 + w;
        int sy1 = sy0 + h;

        if (sx0 < mClipL) sx0 = mClipL;
        if (sy0 < mClipT) sy0 = mClipT;
        if (sx1 > mClipR) sx1 = mClipR;
        if (sy1 > mClipB) sy1 = mClipB;
        if (sx0 >= sx1 || sy0 >= sy1) return;

        int tx0 = sx0 - mTileOrgX;
        int ty0 = sy0 - mTileOrgY;
        int tx1 = sx1 - mTileOrgX;
        int ty1 = sy1 - mTileOrgY;

        if (tx0 < 0) tx0 = 0;
        if (ty0 < 0) ty0 = 0;
        if (tx1 > mTile->width())  tx1 = mTile->width();
        if (ty1 > mTile->height()) ty1 = mTile->height();

        uint16_t* row = mTile->buffer() + ty0 * mTile->stride();

        if (mAlpha == 255) {
            for (int ty = ty0; ty < ty1; ty++) {
                for (int tx = tx0; tx < tx1; tx++) {
                    row[tx] = c.value;
                }
                row += mTile->stride();
            }
        } else {
            uint32_t a  = mAlpha;
            uint32_t ia = 255 - a;
            uint32_t sr = ((c.value >> 11) & 0x1F) * a;
            uint32_t sg = ((c.value >> 5)  & 0x3F) * a;
            uint32_t sb = ( c.value        & 0x1F) * a;

            for (int ty = ty0; ty < ty1; ty++) {
                for (int tx = tx0; tx < tx1; tx++) {
                    uint16_t d = row[tx];
                    uint16_t r = (uint16_t)((sr + ((d >> 11) & 0x1F) * ia) / 255) << 11;
                    uint16_t g = (uint16_t)((sg + ((d >> 5)  & 0x3F) * ia) / 255) << 5;
                    uint16_t b = (uint16_t)((sb + ( d        & 0x1F) * ia) / 255);
                    row[tx] = r | g | b;
                }
                row += mTile->stride();
            }
        }
    }

    // ── drawImage (straight copy, no rotation) ────────────────────

    void drawImage(const void* src, int fmt,
                   int srcW, int srcH, int dx, int dy,
                   const uint8_t* mask = nullptr,
                   const RGB565* tint = nullptr) {

        int sx0 = dx + mScreenX;
        int sy0 = dy + mScreenY;
        int sx1 = sx0 + srcW;
        int sy1 = sy0 + srcH;

        if (sx0 < mClipL) sx0 = mClipL;
        if (sy0 < mClipT) sy0 = mClipT;
        if (sx1 > mClipR) sx1 = mClipR;
        if (sy1 > mClipB) sy1 = mClipB;
        if (sx0 >= sx1 || sy0 >= sy1) return;

        int tx0 = sx0 - mTileOrgX;
        int ty0 = sy0 - mTileOrgY;
        int copyW = sx1 - sx0;
        int copyH = sy1 - sy0;

        if (tx0 < 0) { copyW += tx0; tx0 = 0; }
        if (ty0 < 0) { copyH += ty0; ty0 = 0; }
        if (tx0 + copyW > mTile->width())  copyW = mTile->width()  - tx0;
        if (ty0 + copyH > mTile->height()) copyH = mTile->height() - ty0;
        if (copyW <= 0 || copyH <= 0) return;

        int srcOffX = sx0 - (dx + mScreenX);
        int srcOffY = sy0 - (dy + mScreenY);

        // Fast path: opaque RGB565, no tint, no mask
        if (!tint && fmt == 0 && mAlpha == 255 && !mask) {
            uint16_t* dstBuf = mTile->buffer();
            const uint16_t* src16 = (const uint16_t*)src;
            for (int y = 0; y < copyH; y++) {
                uint16_t* dstRow = dstBuf + (ty0 + y) * mTile->stride() + tx0;
                const uint16_t* srcRow = src16 + (srcOffY + y) * srcW + srcOffX;
                memcpy(dstRow, srcRow, copyW * sizeof(uint16_t));
            }
            return;
        }

        // General path
        uint16_t* dstBuf    = mTile->buffer();
        uint32_t  viewA     = mAlpha;
        int       dstStride = mTile->stride();

        uint32_t tintR = 0, tintG = 0, tintB = 0;
        bool hasTint = (tint != nullptr);
        if (hasTint) {
            tintR = (tint->value >> 11) & 0x1F;
            tintG = (tint->value >> 5)  & 0x3F;
            tintB =  tint->value        & 0x1F;
        }

        const uint16_t* src16 = (const uint16_t*)src;
        const uint8_t*  src8  = (const uint8_t*)src;

        for (int y = 0; y < copyH; y++) {
            int srcY = srcOffY + y;
            uint16_t* dstRow = dstBuf + (ty0 + y) * dstStride + tx0;

            for (int x = 0; x < copyW; x++) {
                int srcX = srcOffX + x;
                uint8_t channelA = 255;
                uint16_t s;
                switch (fmt) {
                case 0: case 1:
                    s = src16[srcY * srcW + srcX]; break;
                case 2:
                    // A8: pixel is alpha, fill color = tint or default black
                    channelA = src8[srcY * srcW + srcX];
                    s = hasTint ? tint->value : 0;
                    break;
                default: s = 0; break;
                }

                if (hasTint && fmt != 2) {
                    uint16_t sr = (s >> 11) & 0x1F;
                    uint16_t sg = (s >> 5)  & 0x3F;
                    uint16_t sb =  s        & 0x1F;
                    s = (((sr + tintR) / 2) << 11) |
                         (((sg + tintG) / 2) << 5)  |
                          ((sb + tintB) / 2);
                }

                uint32_t pixelA = viewA;
                if (fmt == 2)
                    pixelA = (uint32_t)channelA * viewA / 255;
                if (mask && (fmt == 1))
                    pixelA = mask[srcY * srcW + srcX] * viewA / 255;

                uint16_t d = dstRow[x];
                if (pixelA == 255) {
                    dstRow[x] = s;
                } else if (pixelA > 0) {
                    uint32_t ia = 255 - pixelA;
                    uint16_t r = (uint16_t)((((s >> 11) & 0x1F) * pixelA + ((d >> 11) & 0x1F) * ia) / 255) << 11;
                    uint16_t g = (uint16_t)((((s >> 5)  & 0x3F) * pixelA + ((d >> 5)  & 0x3F) * ia) / 255) << 5;
                    uint16_t b = (uint16_t)((( s        & 0x1F) * pixelA + ( d        & 0x1F) * ia) / 255);
                    dstRow[x] = r | g | b;
                }
            }
        }
    }

    // ── rotation scratch buffer ───────────────────────────────────

    // ── drawImageRotated (all rotation, incl. 90° steps) ──────────

    void drawImageRotated(const void* src, int fmt,
                          int srcW, int srcH,
                          int dx, int dy,
                          int rotCx, int rotCy,
                          int16_t angleDeg,
                          const uint8_t* mask   = nullptr,
                          const RGB565* tint   = nullptr,
                          Tile* rotBuffer      = nullptr) {

        if (!resSinTable() && angleDeg % 90 != 0) return;

        angleDeg = angleDeg % 360;
        if (angleDeg < 0) angleDeg += 360;

        // ── Q16 sin/cos: 90° multiples are exact ──
        int32_t cosA, sinA;
        int outW, outH;
        bool useBilinear;  // bilinear only for arbitrary angles
        switch (angleDeg) {
        case 0:   cosA = 65536;  sinA = 0;       outW = srcW; outH = srcH; useBilinear = false; break;
        case 90:  cosA = 0;      sinA = 65536;   outW = srcH; outH = srcW; useBilinear = false; break;
        case 180: cosA = -65536; sinA = 0;       outW = srcW; outH = srcH; useBilinear = false; break;
        case 270: cosA = 0;      sinA = -65536;  outW = srcH; outH = srcW; useBilinear = false; break;
        default:
            cosA = (int32_t)cosDeg(angleDeg) << 1;
            sinA = (int32_t)sinDeg(angleDeg) << 1;
            outW = outH = 0; // computed from bounding box below
            useBilinear = true;
            break;
        }

        // ── rotated bounding-box (works for all angles incl. 90°) ──
        int corners[4][2] = {{0,0}, {srcW,0}, {srcW,srcH}, {0,srcH}};
        int minX = 0x7FFFFFFF, maxX = -0x80000000;
        int minY = 0x7FFFFFFF, maxY = -0x80000000;
        for (int i = 0; i < 4; i++) {
            int rx = ((int32_t)(corners[i][0] - rotCx) * cosA -
                      (int32_t)(corners[i][1] - rotCy) * sinA) >> 16;
            int ry = ((int32_t)(corners[i][0] - rotCx) * sinA +
                      (int32_t)(corners[i][1] - rotCy) * cosA) >> 16;
            if (rx < minX) minX = rx; if (ry < minY) minY = ry;
            if (rx > maxX) maxX = rx; if (ry > maxY) maxY = ry;
        }
        if (angleDeg % 90 != 0) {
            outW = maxX - minX;
            outH = maxY - minY;
        }

        // DDA step vectors (Q16)
        int32_t stepSX_dx =  cosA;
        int32_t stepSY_dx = -sinA;
        int32_t stepSX_dy =  sinA;
        int32_t stepSY_dy =  cosA;

        // ── half-pixel offset ────────────────────────────────────────
        // Output pixel (ox,oy) occupies [ox,ox+1)×[oy,oy+1); its centre
        // is at (ox+0.5, oy+0.5).  The bounding box is built from image
        // corners (not pixel centres), so we must shift the scan origin
        // by +½ pixel (in output space) to sample source pixel centres.
        //
        // R⁻¹(½,½) in Q16:  (½·cos + ½·sin,  -½·sin + ½·cos)
        // Since cosA/sinA are already Q16, ½·v = v / 2:
        int32_t const halfX = (cosA + sinA) / 2;
        int32_t const halfY = (cosA - sinA) / 2;

        // Row-start for output pixel (0,0) → source Q16
        // source = pivot + R⁻¹( (minX+½, minY+½) )
        int32_t baseSX = (int32_t)rotCx * 65536
                       + (int32_t)minX * cosA
                       + (int32_t)minY * sinA
                       + halfX;
        int32_t baseSY = (int32_t)rotCy * 65536
                       - (int32_t)minX * sinA
                       + (int32_t)minY * cosA
                       + halfY;

        // Coordinates outside the source are intentionally left unclamped so
        // the scan can skip transparent/outside parts of the rotated AABB.

        // Decide: rotation buffer or direct path
        bool useRotBuf = (rotBuffer &&
                          rotBuffer->width()  >= outW &&
                          rotBuffer->height() >= outH);

        const uint16_t* src16 = (const uint16_t*)src;
        const uint8_t*  src8  = (const uint8_t*)src;

        uint32_t tintR = 0, tintG = 0, tintB = 0;
        bool hasTint = (tint != nullptr);
        if (hasTint) {
            tintR = (tint->value >> 11) & 0x1F;
            tintG = (tint->value >> 5)  & 0x3F;
            tintB =  tint->value        & 0x1F;
        }

        uint16_t* dstBuf    = useRotBuf ? rotBuffer->buffer() : mTile->buffer();
        int       dstStride = useRotBuf ? rotBuffer->stride() : mTile->stride();

        for (int y = 0; y < outH; y++) {
            int32_t curSX = baseSX;
            int32_t curSY = baseSY;

            uint16_t* dstRow;
            if (useRotBuf) {
                dstRow = dstBuf + y * dstStride;
            } else {
                int sdsty = dy + mScreenY + y;
                if (sdsty < mClipT || sdsty >= mClipB) { baseSX += stepSX_dy; baseSY += stepSY_dy; continue; }
                int tdsty = sdsty - mTileOrgY;
                if (tdsty < 0 || tdsty >= mTile->height()) { baseSX += stepSX_dy; baseSY += stepSY_dy; continue; }
                dstRow = dstBuf + tdsty * dstStride;
            }

            for (int x = 0; x < outW; x++) {
                int sx = curSX >> 16;
                int sy = curSY >> 16;

                if (sx >= -1 && sx < srcW && sy >= -1 && sy < srcH) {

                uint16_t s;
                uint32_t pixelA = 255;

                if (useBilinear) {
                // ── bilinear fetch ─────────────────────────────
                uint32_t fx = (uint32_t)(curSX & 0xFFFF);
                uint32_t fy = (uint32_t)(curSY & 0xFFFF);

                // clamp neighbour indices for border pixels
                int sx0 = (sx < 0) ? 0 : (sx >= srcW ? srcW-1 : sx);
                int sy0 = (sy < 0) ? 0 : (sy >= srcH ? srcH-1 : sy);
                int sx1 = (sx+1 < srcW) ? sx+1 : (sx < 0 ? 0 : srcW-1);
                int sy1 = (sy+1 < srcH) ? sy+1 : (sy < 0 ? 0 : srcH-1);
                if (sx < 0) fx = 0;
                if (sy < 0) fy = 0;

                uint32_t ifx = 65536 - fx;
                uint32_t ify = 65536 - fy;
                uint32_t w00 = (ifx * ify) >> 16;
                uint32_t w10 = ( fx * ify) >> 16;
                uint32_t w01 = (ifx *  fy) >> 16;
                uint32_t w11 = ( fx *  fy) >> 16;

                uint16_t p00, p10, p01, p11;
                uint8_t  a00=255, a10=255, a01=255, a11=255;

                switch (fmt) {
                case 0:
                    p00=src16[sy0*srcW+sx0]; p10=src16[sy0*srcW+sx1];
                    p01=src16[sy1*srcW+sx0]; p11=src16[sy1*srcW+sx1];
                    break;
                case 1:
                    p00=src16[sy0*srcW+sx0]; p10=src16[sy0*srcW+sx1];
                    p01=src16[sy1*srcW+sx0]; p11=src16[sy1*srcW+sx1];
                    if (mask) {
                        a00=mask[sy0*srcW+sx0]; a10=mask[sy0*srcW+sx1];
                        a01=mask[sy1*srcW+sx0]; a11=mask[sy1*srcW+sx1];
                    }
                    break;
                case 2:
                    a00=src8[sy0*srcW+sx0]; a10=src8[sy0*srcW+sx1];
                    a01=src8[sy1*srcW+sx0]; a11=src8[sy1*srcW+sx1];
                    p00=p10=p01=p11=hasTint?tint->value:0;
                    break;
                default: p00=p10=p01=p11=0; break;
                }

                // blend R/G/B channels
                if ((w00|w10|w01|w11) != 0) {
                    uint32_t r0=(p00>>11)&0x1F,g0=(p00>>5)&0x3F,b0=p00&0x1F;
                    uint32_t r1=(p10>>11)&0x1F,g1=(p10>>5)&0x3F,b1=p10&0x1F;
                    uint32_t r2=(p01>>11)&0x1F,g2=(p01>>5)&0x3F,b2=p01&0x1F;
                    uint32_t r3=(p11>>11)&0x1F,g3=(p11>>5)&0x3F,b3=p11&0x1F;
                    uint32_t rb=(r0*w00+r1*w10+r2*w01+r3*w11)>>16;
                    uint32_t gb=(g0*w00+g1*w10+g2*w01+g3*w11)>>16;
                    uint32_t bb=(b0*w00+b1*w10+b2*w01+b3*w11)>>16;
                    if(rb>0x1F)rb=0x1F; if(gb>0x3F)gb=0x3F; if(bb>0x1F)bb=0x1F;
                    s = (uint16_t)((rb<<11)|(gb<<5)|bb);

                    if (fmt==2 || (mask && fmt==1)) {
                        pixelA = (a00*w00+a10*w10+a01*w01+a11*w11)>>16;
                        if(pixelA>255) pixelA=255;
                    }
                } else {
                    s = p00;
                    if (fmt==2 || (mask && fmt==1)) pixelA = a00;
                }

                } else {
                // ── nearest-neighbour fetch ────────────────────
                if (sx >= 0 && sx < srcW && sy >= 0 && sy < srcH) {
                    uint8_t ca = 255;
                    switch (fmt) {
                    case 0: case 1:
                        s = src16[sy * srcW + sx]; break;
                    case 2:
                        ca = src8[sy * srcW + sx];
                        s = hasTint ? tint->value : 0;
                        break;
                    default: s = 0; break;
                    }
                    if (fmt == 2)
                        pixelA = (uint32_t)ca;
                    if (mask && (fmt == 1))
                        pixelA = mask[sy * srcW + sx];
                } else {
                    goto next_pixel;  // out of bounds → skip
                }
                }

                // ── apply tint (common to NN and bilinear) ───────
                if (hasTint && fmt != 2) {
                    uint16_t sr = (s >> 11) & 0x1F;
                    uint16_t sg = (s >> 5)  & 0x3F;
                    uint16_t sb =  s        & 0x1F;
                    s = (uint16_t)((((sr + tintR) / 2) << 11) |
                                   (((sg + tintG) / 2) << 5)  |
                                    ((sb + tintB) / 2));
                }

                if (useRotBuf) {
                    dstRow[x] = s;
                } else {
                    int sdstx = dx + mScreenX + x;
                    if (sdstx < mClipL || sdstx >= mClipR) goto next_pixel;
                    int tdstx = sdstx - mTileOrgX;
                    if (tdstx < 0 || tdstx >= mTile->width()) goto next_pixel;

                    uint16_t d = dstRow[tdstx];
                    if (pixelA == 255) {
                        dstRow[tdstx] = s;
                    } else if (pixelA > 0) {
                        uint32_t ia = 255 - pixelA;
                        uint16_t r = (uint16_t)((((s >> 11) & 0x1F) * pixelA + ((d >> 11) & 0x1F) * ia) / 255) << 11;
                        uint16_t g = (uint16_t)((((s >> 5)  & 0x3F) * pixelA + ((d >> 5)  & 0x3F) * ia) / 255) << 5;
                        uint16_t b = (uint16_t)((( s        & 0x1F) * pixelA + ( d        & 0x1F) * ia) / 255);
                        dstRow[tdstx] = r | g | b;
                    }
                }
                } // if (sx in range)

            next_pixel:
                curSX += stepSX_dx;
                curSY += stepSY_dx;
            }
            baseSX += stepSX_dy;
            baseSY += stepSY_dy;
        }

        // Blit rotation buffer to real destination.
        // WARNING: the DDA wrote into rotBuf using rotBuffer->stride()
        // as the row stride, NOT outW.  We must use the buffer's actual
        // stride when reading, otherwise rows beyond the first are
        // misaligned.
        if (useRotBuf) {
            int sx = dx + mScreenX;
            int sy = dy + mScreenY;
            int dstStride = mTile->stride();
            int srcStride = rotBuffer->stride();
            uint16_t* dstBase = mTile->buffer();
            const uint16_t* srcBase = rotBuffer->buffer();

            for (int y = 0; y < outH; y++) {
                int sdsty = sy + y;
                if (sdsty < mClipT || sdsty >= mClipB) continue;
                int tdsty = sdsty - mTileOrgY;
                if (tdsty < 0 || tdsty >= mTile->height()) continue;

                uint16_t*       dstRow = dstBase + tdsty * dstStride;
                const uint16_t* srcRow = srcBase + y * srcStride;

                for (int x = 0; x < outW; x++) {
                    int sdstx = sx + x;
                    if (sdstx < mClipL || sdstx >= mClipR) continue;
                    int tdstx = sdstx - mTileOrgX;
                    if (tdstx < 0 || tdstx >= mTile->width()) continue;

                    uint16_t s = srcRow[x];
                    if (mAlpha == 255) {
                        dstRow[tdstx] = s;
                    } else {
                        uint32_t a  = mAlpha;
                        uint32_t ia = 255 - a;
                        uint16_t d = dstRow[tdstx];
                        uint16_t r = (uint16_t)((((s >> 11) & 0x1F) * a + ((d >> 11) & 0x1F) * ia) / 255) << 11;
                        uint16_t g = (uint16_t)((((s >> 5)  & 0x3F) * a + ((d >> 5)  & 0x3F) * ia) / 255) << 5;
                        uint16_t b = (uint16_t)((( s        & 0x1F) * a + ( d        & 0x1F) * ia) / 255);
                        dstRow[tdstx] = r | g | b;
                    }
                }
            }
        }
    }

    void copyTile(const Tile& src, int dx, int dy) {
        int sx0 = dx + mScreenX;
        int sy0 = dy + mScreenY;
        int w   = src.width();
        int h   = src.height();
        int sx1 = sx0 + w;
        int sy1 = sy0 + h;

        if (sx0 < mClipL) sx0 = mClipL;
        if (sy0 < mClipT) sy0 = mClipT;
        if (sx1 > mClipR) sx1 = mClipR;
        if (sy1 > mClipB) sy1 = mClipB;
        if (sx0 >= sx1 || sy0 >= sy1) return;

        int tx0   = sx0 - mTileOrgX;
        int ty0   = sy0 - mTileOrgY;
        int copyW = sx1 - sx0;
        int copyH = sy1 - sy0;

        if (tx0 < 0) { copyW += tx0; tx0 = 0; }
        if (ty0 < 0) { copyH += ty0; ty0 = 0; }
        if (tx0 + copyW > mTile->width())  copyW = mTile->width()  - tx0;
        if (ty0 + copyH > mTile->height()) copyH = mTile->height() - ty0;
        if (copyW <= 0 || copyH <= 0) return;

        int srcOffX = sx0 - dx - mScreenX;
        int srcOffY = sy0 - dy - mScreenY;

        if (mAlpha == 255) {
            for (int y = 0; y < copyH; y++) {
                uint16_t*       dstRow = mTile->buffer() + (ty0 + y) * mTile->stride() + tx0;
                const uint16_t* srcRow = src.buffer() + (srcOffY + y) * src.stride() + srcOffX;
                memcpy(dstRow, srcRow, copyW * sizeof(uint16_t));
            }
        } else {
            uint32_t a  = mAlpha;
            uint32_t ia = 255 - a;
            for (int y = 0; y < copyH; y++) {
                uint16_t*       dstRow = mTile->buffer() + (ty0 + y) * mTile->stride() + tx0;
                const uint16_t* srcRow = src.buffer() + (srcOffY + y) * src.stride() + srcOffX;
                for (int x = 0; x < copyW; x++) {
                    uint16_t s = srcRow[x];
                    uint16_t d = dstRow[x];
                    uint16_t r = (uint16_t)((((s >> 11) & 0x1F) * a + ((d >> 11) & 0x1F) * ia) / 255) << 11;
                    uint16_t g = (uint16_t)((((s >> 5)  & 0x3F) * a + ((d >> 5)  & 0x3F) * ia) / 255) << 5;
                    uint16_t b = (uint16_t)((( s        & 0x1F) * a + ( d        & 0x1F) * ia) / 255);
                    dstRow[x] = r | g | b;
                }
            }
        }
    }

private:
    Tile*   mTile     = nullptr;
    int     mTileOrgX = 0;
    int     mTileOrgY = 0;
    int     mScreenX  = 0;
    int     mScreenY  = 0;
    int     mClipL    = -32768;
    int     mClipT    = -32768;
    int     mClipR    = 32767;
    int     mClipB    = 32767;
    uint8_t mAlpha    = 255;
};

} // namespace litho
