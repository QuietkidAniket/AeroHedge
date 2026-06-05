#pragma once
#include <cstdint>

namespace aerohedge {

// Carefully ordered from largest to smallest to avoid compiler padding.
// Total size: 24 bytes. Fits perfectly within a standard 64-byte L1 cache line.
struct MarketTick {
    uint64_t timestamp;     // 8 bytes - Exchange epoch in nanoseconds
    double   price;         // 8 bytes - Underlying asset price
    uint32_t instrument_id; // 4 bytes - Numeric ID for the ticker (e.g., 101 for AAPL)
    uint32_t volume;        // 4 bytes - Traded volume
    uint64_t ingress_cycles;// 8 bytes - No. of tags at the exact moment the data crosses the boundary 
                            // and again right before it leaves the engine
};

} // namespace aerohedge