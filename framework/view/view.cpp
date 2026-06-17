#include "framework/view/view.hpp"
#include "framework/view/view_group.hpp"

namespace litho {

void View::invalidate() {
    if (!mDirtyList) return;

    int sx = mBounds.x;
    int sy = mBounds.y;
    ViewGroup* p = mParent;
    while (p) {
        sx += p->bounds().x;
        sy += p->bounds().y;
        p = p->parent();
    }

    mDirtyList->markDirty({
        (int16_t)sx, (int16_t)sy,
        mBounds.width, mBounds.height
    });
}

bool View::dispatchTouchEvent(TouchEvent& ev, int screenX, int screenY) {
    (void)screenX; (void)screenY;
    return onTouchEvent(ev);
}

} // namespace litho
