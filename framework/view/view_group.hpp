#pragma once
#include "view.hpp"
#include <cstdint>

namespace litho {

class ViewGroup : public View {
public:
    ~ViewGroup() override {
        for (uint16_t i = 0; i < mChildCount; i++) {
            delete mChildren[i];
        }
        delete[] mChildren;
    }

    // ---- child management ----

    void addView(View* child) {
        if (mChildCount >= mCapacity) grow();
        child->mParent    = this;
        child->mDirtyList = mDirtyList;  // propagate from parent
        mChildren[mChildCount++] = child;
    }

    View* childAt(uint16_t i) const {
        return (i < mChildCount) ? mChildren[i] : nullptr;
    }
    uint16_t childCount() const { return mChildCount; }

    // ---- draw ----

    void onDraw(Painter& p) override {
        View::onDraw(p);

        for (uint16_t i = 0; i < mChildCount; i++) {
            View* child = mChildren[i];
            if (!child || !child->visible()) continue;

            int cx = p.screenX() + child->bounds().x;
            int cy = p.screenY() + child->bounds().y;
            int cr = cx + child->bounds().width;
            int cb = cy + child->bounds().height;

            if (!p.intersectsClip(cx, cy, cr, cb)) continue;

            int tx = cx + child->translationX();
            int ty = cy + child->translationY();

            Painter cp = p;
            cp.setScreenOrigin(tx, ty);
            cp.setScreenClip(cx, cy, cr, cb);
            cp.setAlpha((uint8_t)((uint32_t)p.alpha() * child->alpha() / 255));
            child->onDraw(cp);
        }
    }

    // ---- touch dispatch ----

    bool dispatchTouchEvent(TouchEvent& ev, int screenX, int screenY) override {
        // Hit-test children in reverse draw order (topmost first)
        for (int i = mChildCount - 1; i >= 0; i--) {
            View* child = mChildren[i];
            if (!child || !child->visible()) continue;

            int cx = screenX + child->bounds().x;
            int cy = screenY + child->bounds().y;

            if (ev.x >= cx && ev.x < cx + child->bounds().width &&
                ev.y >= cy && ev.y < cy + child->bounds().height) {

                if (child->dispatchTouchEvent(ev, cx, cy)) {
                    if (!ev.handler) {
                        ev.handler   = child;
                        ev.handlerSX = cx;
                        ev.handlerSY = cy;
                    }
                    return true;
                }
            }
        }

        if (onTouchEvent(ev)) {
            if (!ev.handler) {
                ev.handler   = this;
                ev.handlerSX = screenX;
                ev.handlerSY = screenY;
            }
            return true;
        }
        return false;
    }

    void propagateDirtyList(DirtyList* dl) {
        mDirtyList = dl;
        for (uint16_t i = 0; i < mChildCount; i++) {
            if (auto* vg = dynamic_cast<ViewGroup*>(mChildren[i])) {
                vg->propagateDirtyList(dl);
            } else {
                mChildren[i]->mDirtyList = dl;
            }
        }
    }

private:
    void grow() {
        uint16_t newCap = (mCapacity == 0) ? 4 : mCapacity * 2;
        View**   arr    = new View*[newCap];
        for (uint16_t i = 0; i < mChildCount; i++) arr[i] = mChildren[i];
        delete[] mChildren;
        mChildren = arr;
        mCapacity = newCap;
    }

    View**   mChildren   = nullptr;
    uint16_t mChildCount = 0;
    uint16_t mCapacity   = 0;
};

} // namespace litho
