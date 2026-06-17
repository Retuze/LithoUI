#pragma once
#include "port/tick_adapter.hpp"

#include <time.h>

namespace litho {

class X11Tick : public TickAdapter {
public:
    uint32_t tickMs() override {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    }
};

} // namespace litho
