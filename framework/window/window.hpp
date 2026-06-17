#pragma once
#include "framework/view/view_group.hpp"
#include "core/dirty_list.hpp"

namespace litho {

class Window {
public:
    Window()  = default;
    ~Window() { delete mRootView; }

    void setDirtyList(DirtyList* dl) { mDirtyList = dl; }

    void setContentView(ViewGroup* root) {
        delete mRootView;
        mRootView = root;
        if (mDirtyList) mRootView->propagateDirtyList(mDirtyList);
    }

    ViewGroup* rootView() const { return mRootView; }

    void draw(Painter& p) {
        if (mRootView) mRootView->onDraw(p);
    }

    void invalidateRect(const Region& r) {
        if (mDirtyList) mDirtyList->markDirty(r);
    }

    bool dispatchTouchEvent(TouchEvent& ev) {
        if (mRootView) return mRootView->dispatchTouchEvent(ev, 0, 0);
        return false;
    }

private:
    ViewGroup* mRootView  = nullptr;
    DirtyList* mDirtyList = nullptr;
};

} // namespace litho
