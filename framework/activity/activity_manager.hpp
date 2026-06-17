#pragma once
#include "framework/activity/activity.hpp"
#include "framework/window/window_manager.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>

namespace litho {

using ActivityFactory = Activity* (*)();

class ActivityManager {
public:
    static constexpr int kMaxActivities = 8;
    static constexpr int kMaxStack      = 8;

    ActivityManager(WindowManager& wm) : mWindowManager(wm) {}

    // Register an activity class by name. Caller never references the concrete class.
    void registerActivity(const char* name, ActivityFactory factory) {
        if (mRegistryCount >= kMaxActivities) {
            fprintf(stderr, "ActivityManager: registry full\n");
            return;
        }
        mRegistry[mRegistryCount].name    = name;
        mRegistry[mRegistryCount].factory = factory;
        mRegistryCount++;
    }

    template<typename T>
    void registerActivity(const char* name) {
        registerActivity(name, []() -> Activity* { return new T(); });
    }

    void startActivity(Intent& intent) {
        const char* name = intent.target;
        if (!name) {
            fprintf(stderr, "ActivityManager: Intent has no target\n");
            return;
        }

        ActivityFactory factory = findFactory(name);
        if (!factory) {
            fprintf(stderr, "ActivityManager: unknown activity '%s'\n", name);
            return;
        }

        // Pause current
        if (mCount > 0) {
            mStack[mCount - 1]->onPause();
            mStack[mCount - 1]->onStop();
        }

        assert(mCount < kMaxStack && "activity stack overflow");

        Activity* a = factory();
        a->setManager(this);

        Window* win = mWindowManager.createWindow();
        a->setWindow(win);

        a->onCreate(intent.extras);
        a->onStart();
        a->onResume();

        mStack[mCount++] = a;

        win->invalidateRect({0, 0,
            (int16_t)mWindowManager.displayWidth(),
            (int16_t)mWindowManager.displayHeight()});
    }

    void finishActivity(Activity* a) {
        int idx = -1;
        for (int i = 0; i < mCount; i++) {
            if (mStack[i] == a) { idx = i; break; }
        }
        if (idx < 0) return;

        a->onPause();
        a->onStop();
        a->onDestroy();

        Window* w = a->mWindow;
        mWindowManager.destroyWindow(w);

        for (int i = idx; i < mCount - 1; i++) mStack[i] = mStack[i + 1];
        mCount--;
        delete a;

        if (mCount > 0) {
            mStack[mCount - 1]->onStart();
            mStack[mCount - 1]->onResume();
            mStack[mCount - 1]->mWindow->invalidateRect({0, 0,
                (int16_t)mWindowManager.displayWidth(),
                (int16_t)mWindowManager.displayHeight()});
        }
    }

    Activity* currentActivity() {
        return (mCount > 0) ? mStack[mCount - 1] : nullptr;
    }

    WindowManager& windowManager() { return mWindowManager; }

private:
    ActivityFactory findFactory(const char* name) {
        for (int i = 0; i < mRegistryCount; i++) {
            if (strcmp(mRegistry[i].name, name) == 0)
                return mRegistry[i].factory;
        }
        return nullptr;
    }

    struct Entry { const char* name; ActivityFactory factory; };

    WindowManager& mWindowManager;
    Entry          mRegistry[kMaxActivities];
    int            mRegistryCount = 0;
    Activity*      mStack[kMaxStack] = {};
    int            mCount = 0;
};

} // namespace litho
