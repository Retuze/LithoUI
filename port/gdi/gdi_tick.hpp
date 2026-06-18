#pragma once
#include "port/tick_adapter.hpp"

#include <windows.h>

namespace litho {

class GdiTick : public TickAdapter {
public:
    uint32_t tickMs() override {
        return (uint32_t)GetTickCount64();
    }
};

} // namespace litho
