#ifndef TIMER_HPP
#define TIMER_HPP

#include <chrono>
#include <string>
#include <iostream>

namespace trading {
namespace utils {

class Timer {
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}

    // Reset the timer
    void reset() {
        start_ = std::chrono::high_resolution_clock::now();
    }

    // Get elapsed time in nanoseconds
    uint64_t elapsedNanos() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            now - start_
        ).count();
    }

    // Get elapsed time in microseconds
    uint64_t elapsedMicros() const {
        return elapsedNanos() / 1000;
    }

    // Get elapsed time in milliseconds
    uint64_t elapsedMillis() const {
        return elapsedNanos() / 1000000;
    }

    // Get elapsed time in seconds
    double elapsedSeconds() const {
        return elapsedNanos() / 1000000000.0;
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
};

// RAII timer that prints elapsed time on destruction
class ScopedTimer {
public:
    explicit ScopedTimer(const std::string& name) 
        : name_(name), timer_() {}

    ~ScopedTimer() {
        auto elapsed = timer_.elapsedMicros();
        std::cout << "[" << name_ << "] took " << elapsed << " µs" << std::endl;
    }

private:
    std::string name_;
    Timer timer_;
};

// High-precision CPU cycle counter (x86/x64 only)
#ifdef __x86_64__
inline uint64_t rdtsc() {
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
#else
inline uint64_t rdtsc() {
    // Fallback to chrono for non-x86 platforms
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()
    ).count();
}
#endif

// Latency measurement using CPU cycles
class LatencyMeasurer {
public:
    void start() {
        startCycles_ = rdtsc();
    }

    uint64_t end() {
        uint64_t endCycles = rdtsc();
        return endCycles - startCycles_;
    }

    // Estimate nanoseconds (assuming 2.5 GHz CPU, adjust as needed)
    double cyclesToNanos(uint64_t cycles, double cpuGHz = 2.5) const {
        return cycles / cpuGHz;
    }

private:
    uint64_t startCycles_;
};

} // namespace utils
} // namespace trading

#endif // TIMER_HPP