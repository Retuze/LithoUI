#pragma once
#include "framework/view/view.hpp"

namespace litho {

class ImageView : public View {
public:
    ImageView(int w, int h)
        : mWidth(w), mHeight(h) {}

    void onDraw(Painter& p) override {
        // Placeholder: draw a muted green rect (distinct from text gray)
        p.fillRect(0, 0, mWidth, mHeight, RGB565::fromRGB(64, 160, 64));
    }

    // TODO: setImage(...), image decoding / scaling

private:
    int mWidth;
    int mHeight;
};

} // namespace litho
