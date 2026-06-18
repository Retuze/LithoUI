#include "framework/view/view.hpp"
#include "framework/view/view_group.hpp"
#include "framework/animation/view_property_animator.hpp"

namespace litho {

View::~View() {
    delete mAnimator;
}

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
    if (onTouchEvent(ev)) {
        ev.handler   = this;
        ev.handlerSX = screenX;
        ev.handlerSY = screenY;
        return true;
    }
    return false;
}

ViewPropertyAnimator& View::animate() {
    if (mAnimator) {
        delete mAnimator;
        mAnimator = nullptr;
    }
    mAnimator = new ViewPropertyAnimator(this);
    return *mAnimator;
}

} // namespace litho
