#pragma once

#include <chrono>
#include <cstdint>

#if defined(__x86_64__) || defined(_M_X64)
  #if defined(_MSC_VER)
    #include <intrin.h>
  #else
    #include <x86intrin.h>
  #endif
#endif

namespace leasing::common::ring {
class StopWatch {
public:
    using clock = std::chrono::steady_clock;

    StopWatch() : start_(clock::now()) {}

    void reset() { start_ = clock::now(); }

    double elapsed_ms() const {
        const auto now = clock::now();
        return std::chrono::duration<double, std::milli>(now - start_).count();
    }

    double elapsed_us() const {
        const auto now = clock::now();
        return std::chrono::duration<double, std::micro>(now - start_).count();
    }

    std::int64_t elapsed_ns() const {
        const auto now = clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   now - start_).count();
    }

private:
    clock::time_point start_;
};

// Cycle counter.
#if defined(__x86_64__) || defined(_M_X64)
inline std::uint64_t rdtsc() noexcept { return __rdtsc(); }
#endif

} 
