#pragma once
#include "tile.hpp"
#include "region.hpp"
#include "painter.hpp"
#include <cstdint>
#include <cstdio>

namespace litho {

// PFB: divides the screen into fixed-size blocks with a small tile buffer pool.
// Renders a region by iterating the blocks covering it — each block gets a Painter
// set up with the correct tile, screen origin, and clip.

class PFB {
public:
    bool init(int blockW, int blockH, int poolSize,
              int screenW, int screenH) {
        mBlockW   = blockW;
        mBlockH   = blockH;
        mScreenW  = screenW;
        mScreenH  = screenH;
        mCols     = (screenW + blockW - 1) / blockW;
        mRows     = (screenH + blockH - 1) / blockH;

        int pixPerBlock = blockW * blockH;
        mPoolSize  = poolSize;
        mPoolTiles = new Tile[poolSize];
        mPoolBufs  = new uint16_t[pixPerBlock * poolSize];
        mPoolUsed  = new bool[poolSize]();

        for (int i = 0; i < poolSize; i++) {
            mPoolTiles[i].attach(mPoolBufs + i * pixPerBlock,
                                  blockW, blockH);
        }

        printf("PFB: %dx%d blocks, %dx%d grid, %d pool (%d bytes)\n",
               blockW, blockH, mCols, mRows,
               poolSize, pixPerBlock * poolSize * 2);
        return true;
    }

    ~PFB() {
        delete[] mPoolTiles;
        delete[] mPoolBufs;
        delete[] mPoolUsed;
    }

    int blockW() const { return mBlockW; }
    int blockH() const { return mBlockH; }
    int cols()   const { return mCols; }
    int rows()   const { return mRows; }

    // Iterate the blocks covering a screen region. Calls `draw` for each block
    // with a ready-to-use Painter and block metadata. Flushes each block.
    template<typename Display, typename DrawFn>
    void drawRegion(const Region& region, Display& display, DrawFn&& draw) {
        int c0 = region.x / mBlockW;
        int r0 = region.y / mBlockH;
        int c1 = (region.x + region.width  + mBlockW  - 1) / mBlockW;
        int r1 = (region.y + region.height + mBlockH - 1) / mBlockH;
        if (c0 < 0)     c0 = 0;
        if (r0 < 0)     r0 = 0;
        if (c1 > mCols) c1 = mCols;
        if (r1 > mRows) r1 = mRows;

        Painter painter;

        for (int row = r0; row < r1; row++) {
            for (int col = c0; col < c1; col++) {
                int bx = col * mBlockW;
                int by = row * mBlockH;
                int bw = blockActualW(col);
                int bh = blockActualH(row);

                Tile& tile = acquireTile();

                painter.setTile(tile, bx, by);
                painter.setScreenOrigin(0, 0);
                painter.setScreenClip(bx, by, bx + bw, by + bh);

                draw(painter, bx, by, bw, bh);

                display.bitblt(tile.buffer(), bx, by, bw, bh);
                releaseTile(tile);
            }
        }
    }

private:
    int blockActualW(int col) const {
        int x = col * mBlockW;
        return (x + mBlockW <= mScreenW) ? mBlockW : mScreenW - x;
    }
    int blockActualH(int row) const {
        int y = row * mBlockH;
        return (y + mBlockH <= mScreenH) ? mBlockH : mScreenH - y;
    }

    Tile& acquireTile() {
        for (int i = 0; i < mPoolSize; i++) {
            if (!mPoolUsed[i]) {
                mPoolUsed[i] = true;
                return mPoolTiles[i];
            }
        }
        return mPoolTiles[0]; // pool exhausted — shouldn't happen
    }

    void releaseTile(Tile& tile) {
        for (int i = 0; i < mPoolSize; i++) {
            if (&mPoolTiles[i] == &tile) {
                mPoolUsed[i] = false;
                return;
            }
        }
    }

    int      mBlockW  = 0;
    int      mBlockH  = 0;
    int      mScreenW = 0;
    int      mScreenH = 0;
    int      mCols    = 0;
    int      mRows    = 0;
    int      mPoolSize = 0;

    Tile*    mPoolTiles = nullptr;
    uint16_t* mPoolBufs = nullptr;
    bool*    mPoolUsed  = nullptr;
};

} // namespace litho
