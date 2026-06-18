#pragma once
#include "port/display_adapter.hpp"

#include <windows.h>
#include <cstdio>

namespace litho {

class GdiDisplay : public DisplayAdapter {
public:
    ~GdiDisplay() override {
        if (mMemDC)    { DeleteDC(mMemDC); }
        if (mDibBitmap) { DeleteObject(mDibBitmap); }
        if (mHwnd)     { DestroyWindow(mHwnd); }
        if (mWcAtom)   { UnregisterClassW(mClassName, mInstance); }
    }

    bool init(int w, int h) override {
        mWidth  = w;
        mHeight = h;

        mInstance = GetModuleHandle(nullptr);

        // Register window class
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(WNDCLASSEXW);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = sWndProc;
        wc.hInstance     = mInstance;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = mClassName;

        mWcAtom = RegisterClassExW(&wc);
        if (!mWcAtom) {
            fprintf(stderr, "GdiDisplay: RegisterClassEx failed\n");
            return false;
        }

        // Adjust window rect to get the desired client area
        RECT rect = { 0, 0, mWidth, mHeight };
        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        AdjustWindowRect(&rect, style, FALSE);

        mHwnd = CreateWindowExW(0, mClassName, L"LithoUI",
                                style,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                rect.right - rect.left, rect.bottom - rect.top,
                                nullptr, nullptr, mInstance, nullptr);
        if (!mHwnd) {
            fprintf(stderr, "GdiDisplay: CreateWindowEx failed\n");
            return false;
        }

        ShowWindow(mHwnd, SW_SHOW);
        UpdateWindow(mHwnd);

        // Create DIB section as composition buffer (32-bit XRGB)
        HDC hdc = GetDC(mHwnd);
        mMemDC = CreateCompatibleDC(hdc);

        BITMAPINFO bi = {};
        bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth       = mWidth;
        bi.bmiHeader.biHeight      = -mHeight;  // top-down
        bi.bmiHeader.biPlanes      = 1;
        bi.bmiHeader.biBitCount    = 32;
        bi.bmiHeader.biCompression = BI_RGB;

        mDibBitmap = CreateDIBSection(mMemDC, &bi, DIB_RGB_COLORS,
                                      (void**)&mCompBuf, nullptr, 0);
        if (!mDibBitmap) {
            fprintf(stderr, "GdiDisplay: CreateDIBSection failed\n");
            ReleaseDC(mHwnd, hdc);
            return false;
        }

        SelectObject(mMemDC, mDibBitmap);
        ReleaseDC(mHwnd, hdc);

        // Clear to white
        memset(mCompBuf, 0xFF, mWidth * mHeight * 4);

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
        HDC hdc = GetDC(mHwnd);
        BitBlt(hdc, 0, 0, mWidth, mHeight, mMemDC, 0, 0, SRCCOPY);
        ReleaseDC(mHwnd, hdc);

        if (mCaptureOn) saveFrame();
    }

    void enableCapture(const char* path) { mCaptureOn = true; mCapturePath = path; mFrameIdx = 0; }
    void disableCapture()                { mCaptureOn = false; }

    int width()  const override { return mWidth; }
    int height() const override { return mHeight; }

    HWND hwnd() const { return mHwnd; }

private:
    static LRESULT CALLBACK sWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }
    }

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

    static constexpr const wchar_t* mClassName = L"LithoGdiWindow";

    HINSTANCE mInstance    = nullptr;
    ATOM      mWcAtom      = 0;
    HWND      mHwnd        = nullptr;
    HDC       mMemDC       = nullptr;
    HBITMAP   mDibBitmap   = nullptr;
    uint32_t* mCompBuf     = nullptr;
    int       mWidth       = 0;
    int       mHeight      = 0;
    bool      mCaptureOn   = false;
    const char* mCapturePath = nullptr;
    int         mFrameIdx    = 0;
};

} // namespace litho
