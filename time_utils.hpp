#pragma once
#include <cstdint>
#include <chrono>
#include <thread>
#include <iostream>

#if defined(__APPLE__)
#include <mach/mach_time.h>
#elif defined(__x86_64__) || defined(_M_X64)
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#endif

namespace aerohedge {

class TSCClock {
private:
    double cycles_per_ns_;

public:
    TSCClock() {
        calibrate();
    }

    // Force inline so the compiler embeds this directly without a function call overhead
    inline uint64_t rdtsc() const {
#if defined(__APPLE__)
        // Apple's direct hardware time register
        return mach_absolute_time(); 
#elif defined(__x86_64__) || defined(_M_X64)
        // x86 standard for Linux/Windows trading servers
        return __rdtsc();
#else
        // Fallback for unknown architectures
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
#endif
    }

    inline double cycles_to_ns(uint64_t cycles) const {
        return cycles / cycles_per_ns_;
    }

private:
    void calibrate() {
        std::cout << "[Clock]: Calibrating CPU cycle counter...\n";
        
        // Measure how many CPU cycles happen in exactly 1 second
        uint64_t start_cycles = rdtsc();
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        uint64_t end_cycles = rdtsc();
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
        uint64_t elapsed_cycles = end_cycles - start_cycles;
        
        cycles_per_ns_ = static_cast<double>(elapsed_cycles) / elapsed_ns;
        
        std::cout << "[Clock]: Calibration complete. CPU runs at roughly " 
                  << (cycles_per_ns_ * 1000.0) << " MHz.\n";
    }
};

// Global clock instance
extern TSCClock global_clock;

} // namespace aerohedge