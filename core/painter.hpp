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
        // reset clip to "unbounded" for the new tile
        mClipL = -32768; mClipT = -32768;
        mClipR =  32767; mClipB =  32767;
    }

    void setScreenOrigin(int sx, int sy) { mScreenX = sx; mScreenY = sy; }
    int  screenX() const { return mScreenX; }
    int  screenY() const { return mScreenY; }

    bool intersectsClip(int left, int top, int right, int bottom) const {
        return left < mClipR && right > mClipL &&
               top  < mClipB && bottom > mClipT;
    }

    // Clip in screen coords. Intersects with existing clip — tightens down the tree.
    void setScreenClip(int left, int top, int right, int bottom) {
        if (left   > mClipL) mClipL = left;
        if (top    > mClipT) mClipT = top;
        if (right  < mClipR) mClipR = right;
        if (bottom < mClipB) mClipB = bottom;
    }

    // Fill in local coords → screen → clip → tile.
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
        for (int ty = ty0; ty < ty1; ty++) {
            for (int tx = tx0; tx < tx1; tx++) {
                row[tx] = c.value;
            }
            row += mTile->stride();
        }
    }

    // Copy src tile at local (dx, dy). Clipped the same way as fillRect.
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

        // source pixel (0,0) maps to screen (dx+mScreenX, dy+mScreenY).
        // After clipping, the first drawn screen pixel is (sx0, sy0),
        // which corresponds to source pixel (sx0 - dx - mScreenX, sy0 - dy - mScreenY).
        int srcOffX = sx0 - dx - mScreenX;
        int srcOffY = sy0 - dy - mScreenY;

        for (int y = 0; y < copyH; y++) {
            uint16_t*       dstRow = mTile->buffer() + (ty0 + y) * mTile->stride() + tx0;
            const uint16_t* srcRow = src.buffer() + (srcOffY + y) * src.stride() + srcOffX;
            memcpy(dstRow, srcRow, copyW * sizeof(uint16_t));
        }
    }

private:
    Tile* mTile     = nullptr;
    int   mTileOrgX = 0;
    int   mTileOrgY = 0;
    int   mScreenX  = 0;
    int   mScreenY  = 0;
    int   mClipL    = -32768;
    int   mClipT    = -32768;
    int   mClipR    = 32767;
    int   mClipB    = 32767;
};

} // namespace litho
