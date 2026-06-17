#pragma once
#include "core/region.hpp"
#include "core/painter.hpp"
#include "core/dirty_list.hpp"
#include "framework/base/object.hpp"
#include "framework/event/event_types.hpp"

namespace litho {

class ViewGroup;

class View : public Object {
public:
    View()  = default;
    ~View() override = default;

    Region&       bounds()       { return mBounds; }
    const Region& bounds() const { return mBounds; }

    int  x()      const { return mBounds.x; }
    int  y()      const { return mBounds.y; }
    int  width()  const { return mBounds.width; }
    int  height() const { return mBounds.height; }

    ViewGroup* parent()    const { return mParent; }
    bool       visible()   const { return bVisible; }
    void       setVisible(bool v) { bVisible = v; }

    virtual void onDraw(Painter& p) { (void)p; }

    // Walk up parent chain to compute screen coords, then push to DirtyList.
    void invalidate();

    // Touch dispatch. Default: no children, just call onTouchEvent.
    virtual bool dispatchTouchEvent(TouchEvent& ev, int screenX, int screenY);

    // Touch event. Return true if handled.
    virtual bool onTouchEvent(TouchEvent& e) { (void)e; return false; }

protected:
    friend class ViewGroup;

    Region     mBounds;
    bool       bVisible    = true;
    ViewGroup* mParent     = nullptr;
    DirtyList* mDirtyList  = nullptr;
};

} // namespace litho
