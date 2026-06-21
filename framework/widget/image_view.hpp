#pragma once
#include "framework/view/view.hpp"
#include "res_images.h"

namespace litho {

class ImageView : public View {
public:
    ImageView(int w, int h) : mWidth(w), mHeight(h) {}

    // ---- image source ----

    void setImageId(ImageId id) {
        mImageId = id;
        invalidate();
    }
    ImageId imageId() const { return mImageId; }

    // ---- tint ----

    void setTintColor(RGB565 color) { mTint = color; mHasTint = true; invalidate(); }
    void clearTint()                { mHasTint = false;          invalidate(); }
    bool hasTint()      const { return mHasTint; }
    RGB565 tintColor()  const { return mTint; }

    // ---- rotation ----

    void setRotationAngle(int16_t degrees, int pivotX, int pivotY) {
        if (mHasAngle && mAngleDeg == degrees &&
            mPivotX == pivotX && mPivotY == pivotY) {
            return;
        }

        Region old = screenBounds();
        mAngleDeg = degrees;
        mPivotX   = pivotX;
        mPivotY   = pivotY;
        mHasAngle = true;
        invalidate();
        if (mDirtyList) mDirtyList->markDirty(old);
    }
    // Default pivot = view center
    void setRotationAngle(int16_t degrees) {
        setRotationAngle(degrees, mWidth / 2, mHeight / 2);
    }
    bool    hasRotationAngle() const { return mHasAngle; }
    int16_t rotationAngleDeg() const { return mAngleDeg; }
    int     rotationPivotX()   const { return mPivotX; }
    int     rotationPivotY()   const { return mPivotY; }
    void    clearRotation() {
        if (!mHasAngle) return;

        Region old = screenBounds();
        mHasAngle = false;
        invalidate();
        if (mDirtyList) mDirtyList->markDirty(old);
    }

    // ---- transformed bounds for ViewGroup clip ──────────────────

    Region transformedBounds() const override {
        if (!mHasAngle) return View::transformedBounds();

        // Start with base class (bounds + translation)
        Region base = View::transformedBounds();

        // Rotate 4 corners around pivot, take bounding box
        int   w = mBounds.width, h = mBounds.height;
        int16_t a = mAngleDeg % 360;
        if (a < 0) a += 360;

        if (a == 0) return base;

        int32_t cosA, sinA;
        switch (a) {
        case 90:  cosA = 0;      sinA = 65536;  break;
        case 180: cosA = -65536; sinA = 0;      break;
        case 270: cosA = 0;      sinA = -65536; break;
        default:
            cosA = (int32_t)cosDeg(a) << 1;
            sinA = (int32_t)sinDeg(a) << 1;
            break;
        }

        int corners[4][2] = {{0,0}, {w,0}, {w,h}, {0,h}};
        int minX = 0x7FFFFFFF, maxX = -0x80000000;
        int minY = 0x7FFFFFFF, maxY = -0x80000000;
        for (int i = 0; i < 4; i++) {
            int dx = corners[i][0] - mPivotX;
            int dy = corners[i][1] - mPivotY;
            int rx = (dx * cosA - dy * sinA) >> 16;
            int ry = (dx * sinA + dy * cosA) >> 16;
            if (rx < minX) minX = rx; if (ry < minY) minY = ry;
            if (rx > maxX) maxX = rx; if (ry > maxY) maxY = ry;
        }
        return {(int16_t)(base.x + mPivotX + minX),
                (int16_t)(base.y + mPivotY + minY),
                (int16_t)(maxX - minX),   (int16_t)(maxY - minY)};
    }

    // ---- draw ----

    void onDraw(Painter& p) override {
        if (mImageId < IMG_COUNT) {
            const ImageEntry* e     = imageEntry(mImageId);
            const void*       src   = (const void*)imagePixels(mImageId);
            const uint8_t*    alpha = imageAlpha(mImageId);
            const RGB565*     tint  = mHasTint ? &mTint : nullptr;

            if (mHasAngle) {
                p.drawImageRotated(src, e->format, e->width, e->height,
                                   0, 0, mPivotX, mPivotY, mAngleDeg,
                                   alpha, tint);
            } else {
                p.drawImage(src, e->format, e->width, e->height, 0, 0,
                            alpha, tint);
            }
        } else {
            p.fillRect(0, 0, mWidth, mHeight, RGB565::fromRGB(64, 160, 64));
        }
    }

private:
    int       mWidth    = 0;
    int       mHeight   = 0;
    ImageId   mImageId  = IMG_COUNT;
    RGB565    mTint     = {0};
    bool      mHasTint  = false;
    bool      mHasAngle = false;
    int16_t   mAngleDeg = 0;
    int       mPivotX   = 0;
    int       mPivotY   = 0;
};

} // namespace litho
