#pragma once
#include "framework/view/view.hpp"
#include "generated/res_bundle.h"

namespace litho {

class ImageView : public View {
public:
    ImageView(int w, int h) : mWidth(w), mHeight(h) {}

    void setImage(const ImageAsset* asset) {
        mAsset = asset;
        invalidate();
    }

    const ImageAsset* image() const { return mAsset; }

    void onDraw(Painter& p) override {
        if (mAsset) {
            p.copyPixels(mAsset->pixels, mAsset->alpha,
                         mAsset->width, mAsset->height, 0, 0);
        } else {
            p.fillRect(0, 0, mWidth, mHeight, RGB565::fromRGB(64, 160, 64));
        }
    }

private:
    int              mWidth  = 0;
    int              mHeight = 0;
    const ImageAsset* mAsset = nullptr;
};

} // namespace litho
