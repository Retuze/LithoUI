#pragma once
#include "port/display_adapter.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdio>
#include <cstring>

namespace litho {

class X11Display : public DisplayAdapter {
public:
    ~X11Display() override {
        if (mGC)         { XFreeGC(mDisplay, mGC); }
        if (mXWindow)    { XDestroyWindow(mDisplay, mXWindow); }
        if (mDisplay)    { XCloseDisplay(mDisplay); }
        if (mXImage)     { mXImage->data = nullptr; XDestroyImage(mXImage); }
        delete[] mCompBuf;
    }

    bool init(int w, int h) override {
        mWidth  = w;
        mHeight = h;

        mDisplay = XOpenDisplay(nullptr);
        if (!mDisplay) {
            fprintf(stderr, "X11Display: cannot open display\n");
            return false;
        }

        int screen = DefaultScreen(mDisplay);

        mXWindow = XCreateSimpleWindow(mDisplay,
                                        RootWindow(mDisplay, screen),
                                        0, 0, mWidth, mHeight, 0,
                                        BlackPixel(mDisplay, screen),
                                        WhitePixel(mDisplay, screen));

        XSelectInput(mDisplay, mXWindow,
                     ExposureMask | KeyPressMask | KeyReleaseMask |
                     ButtonPressMask | ButtonReleaseMask |
                     PointerMotionMask | StructureNotifyMask);

        XMapWindow(mDisplay, mXWindow);
        XFlush(mDisplay);

        XEvent e;
        while (true) {
            XNextEvent(mDisplay, &e);
            if (e.type == Expose) break;
        }

        mGC = XCreateGC(mDisplay, mXWindow, 0, nullptr);

        // full-screen composition buffer (XRGB8888)
        mCompBuf  = new uint32_t[mWidth * mHeight]();
        mXImage   = XCreateImage(mDisplay,
                                  DefaultVisual(mDisplay, screen),
                                  DefaultDepth(mDisplay, screen),
                                  ZPixmap, 0, (char*)mCompBuf,
                                  mWidth, mHeight, 32, mWidth * 4);
        mXImage->byte_order = LSBFirst;

        return true;
    }

    void bitblt(const uint16_t* data, int x, int y, int w, int h) override {
        if (w <= 0 || h <= 0) return;

        for (int row = 0; row < h; row++) {
            uint32_t* dst = mCompBuf + (y + row) * mWidth + x;
            const uint16_t* src = data + row * w;
            for (int col = 0; col < w; col++) {
                uint16_t p = src[col];
                uint8_t  r = (p >> 11) & 0x1F;
                uint8_t  g = (p >> 5)  & 0x3F;
                uint8_t  b =  p        & 0x1F;
                dst[col] = ((uint32_t)(r * 255 / 31) << 16) |
                           ((uint32_t)(g * 255 / 63) << 8)  |
                           ((uint32_t)(b * 255 / 31));
            }
        }
    }

    void flush() override {
        XPutImage(mDisplay, mXWindow, mGC, mXImage, 0, 0, 0, 0, mWidth, mHeight);
        XFlush(mDisplay);

        if (mCaptureOn) saveFrame();
    }

    // ---- debug: frame capture ----

    void enableCapture(const char* path) { mCaptureOn = true; mCapturePath = path; mFrameIdx = 0; }
    void disableCapture()                { mCaptureOn = false; }

    int width()  const override { return mWidth; }
    int height() const override { return mHeight; }

    Display* xDisplay() const { return mDisplay; }
    ::Window xWindow()  const { return mXWindow; }

private:
    void saveFrame() {
        char path[256];
        snprintf(path, sizeof(path), "%s_%04d.ppm", mCapturePath, mFrameIdx++);
        FILE* f = fopen(path, "wb");
        if (!f) return;

        fprintf(f, "P6\n%d %d\n255\n", mWidth, mHeight);
        for (int i = 0; i < mWidth * mHeight; i++) {
            uint32_t p = mCompBuf[i];
            uint8_t  rgb[3] = { (uint8_t)(p >> 16), (uint8_t)(p >> 8), (uint8_t)p };
            fwrite(rgb, 3, 1, f);
        }
        fclose(f);
    }

    Display*  mDisplay  = nullptr;
    ::Window  mXWindow  = 0;
    GC        mGC       = nullptr;
    XImage*   mXImage   = nullptr;
    uint32_t* mCompBuf  = nullptr;
    int       mWidth    = 0;
    int       mHeight   = 0;
    bool      mCaptureOn  = false;
    const char* mCapturePath = nullptr;
    int         mFrameIdx    = 0;
};

} // namespace litho
