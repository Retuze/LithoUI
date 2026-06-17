#pragma once
#include <cstdint>

namespace litho {

class TickAdapter {
public:
    virtual ~TickAdapter() = default;
    virtual uint32_t tickMs() = 0;
};

} // namespace litho
