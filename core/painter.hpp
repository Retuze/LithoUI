#pragma once
#include "tile.hpp"
#include "litho_core.h"
#include <cstring>

namespace litho {

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

    // Copy external pixels with optional per-pixel alpha mask.
    void copyPixels(const uint16_t* src, const uint8_t* mask,
                    int srcW, int srcH, int dx, int dy) {
        int sx0 = dx + mScreenX;
        int sy0 = dy + mScreenY;
        int sx1 = sx0 + srcW;
        int sy1 = sy0 + srcH;

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

        uint16_t* dstBuf = mTile->buffer();

        if (mAlpha == 255 && !mask) {
            for (int y = 0; y < copyH; y++) {
                uint16_t*       dstRow = dstBuf + (ty0 + y) * mTile->stride() + tx0;
                const uint16_t* srcRow = src + (srcOffY + y) * srcW + srcOffX;
                memcpy(dstRow, srcRow, copyW * sizeof(uint16_t));
            }
            return;
        }

        uint32_t viewA = mAlpha;
        for (int y = 0; y < copyH; y++) {
            uint16_t*       dstRow  = dstBuf + (ty0 + y) * mTile->stride() + tx0;
            const uint16_t* srcRow  = src + (srcOffY + y) * srcW + srcOffX;
            const uint8_t*  maskRow = mask ? (mask + (srcOffY + y) * srcW + srcOffX) : nullptr;

            for (int x = 0; x < copyW; x++) {
                uint16_t s = srcRow[x];
                uint16_t d = dstRow[x];
                uint32_t a = maskRow ? (maskRow[x] * viewA / 255) : viewA;

                if (a == 255) {
                    dstRow[x] = s;
                } else if (a > 0) {
                    uint32_t ia = 255 - a;
                    uint16_t r = (uint16_t)((((s >> 11) & 0x1F) * a + ((d >> 11) & 0x1F) * ia) / 255) << 11;
                    uint16_t g = (uint16_t)((((s >> 5)  & 0x3F) * a + ((d >> 5)  & 0x3F) * ia) / 255) << 5;
                    uint16_t b = (uint16_t)((( s        & 0x1F) * a + ( d        & 0x1F) * ia) / 255);
                    dstRow[x] = r | g | b;
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
