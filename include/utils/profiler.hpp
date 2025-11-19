#ifndef PROFILER_HPP
#define PROFILER_HPP

#include "utils/timer.hpp"
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace trading {
namespace utils {

/**
 * LatencyStats - Statistical analysis of latency measurements.
 */
class LatencyStats {
public:
    void record(uint64_t latencyNs) {
        samples_.push_back(latencyNs);
        sum_ += latencyNs;
        count_++;
        
        if (latencyNs < min_) min_ = latencyNs;
        if (latencyNs > max_) max_ = latencyNs;
    }

    void recordCycles(uint64_t cycles) {
        cycleSamples_.push_back(cycles);
        cycleSum_ += cycles;
    }

    uint64_t getMin() const { return min_; }
    uint64_t getMax() const { return max_; }
    uint64_t getCount() const { return count_; }
    
    double getAverage() const {
        return count_ > 0 ? static_cast<double>(sum_) / count_ : 0.0;
    }

    double getAverageCycles() const {
        return cycleSamples_.empty() ? 0.0 : 
               static_cast<double>(cycleSum_) / cycleSamples_.size();
    }

    uint64_t getPercentile(double percentile) {
        if (samples_.empty()) return 0;
        
        std::vector<uint64_t> sorted = samples_;
        std::sort(sorted.begin(), sorted.end());
        
        size_t index = static_cast<size_t>(
            (percentile / 100.0) * (sorted.size() - 1)
        );
        return sorted[index];
    }

    double getStdDev() const {
        if (count_ < 2) return 0.0;
        
        double mean = getAverage();
        double variance = 0.0;
        
        for (uint64_t sample : samples_) {
            double diff = sample - mean;
            variance += diff * diff;
        }
        
        return std::sqrt(variance / (count_ - 1));
    }

    std::string toString() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        oss << "Samples: " << count_ << "\n";
        oss << "Min: " << min_ << " ns\n";
        oss << "Max: " << max_ << " ns\n";
        oss << "Avg: " << getAverage() << " ns\n";
        oss << "StdDev: " << getStdDev() << " ns\n";
        oss << "P50: " << const_cast<LatencyStats*>(this)->getPercentile(50) << " ns\n";
        oss << "P95: " << const_cast<LatencyStats*>(this)->getPercentile(95) << " ns\n";
        oss << "P99: " << const_cast<LatencyStats*>(this)->getPercentile(99) << " ns\n";
        oss << "P99.9: " << const_cast<LatencyStats*>(this)->getPercentile(99.9) << " ns\n";
        
        if (!cycleSamples_.empty()) {
            oss << "Avg Cycles: " << getAverageCycles() << "\n";
        }
        
        return oss.str();
    }

    void clear() {
        samples_.clear();
        cycleSamples_.clear();
        sum_ = 0;
        cycleSum_ = 0;
        count_ = 0;
        min_ = UINT64_MAX;
        max_ = 0;
    }

private:
    std::vector<uint64_t> samples_;
    std::vector<uint64_t> cycleSamples_;
    uint64_t sum_ = 0;
    uint64_t cycleSum_ = 0;
    uint64_t count_ = 0;
    uint64_t min_ = UINT64_MAX;
    uint64_t max_ = 0;
};

/**
 * Profiler - Comprehensive performance profiling tool.
 */
class Profiler {
public:
    static Profiler& getInstance() {
        static Profiler instance;
        return instance;
    }

    void startSection(const std::string& name) {
        timers_[name].reset();
    }

    void endSection(const std::string& name) {
        auto it = timers_.find(name);
        if (it != timers_.end()) {
            uint64_t elapsed = it->second.elapsedNanos();
            stats_[name].record(elapsed);
        }
    }

    void recordLatency(const std::string& name, uint64_t latencyNs) {
        stats_[name].record(latencyNs);
    }

    void recordCycles(const std::string& name, uint64_t cycles) {
        stats_[name].recordCycles(cycles);
    }

    const LatencyStats& getStats(const std::string& name) const {
        static LatencyStats empty;
        auto it = stats_.find(name);
        return it != stats_.end() ? it->second : empty;
    }

    std::string getReport() const {
        std::ostringstream oss;
        oss << "\n========== PERFORMANCE PROFILE ==========\n\n";
        
        for (const auto& [name, stats] : stats_) {
            oss << "--- " << name << " ---\n";
            oss << stats.toString();
            oss << "\n";
        }
        
        oss << "=========================================\n";
        return oss.str();
    }

    void clear() {
        stats_.clear();
        timers_.clear();
    }

    void clearSection(const std::string& name) {
        stats_[name].clear();
    }

private:
    Profiler() = default;
    
    std::map<std::string, Timer> timers_;
    std::map<std::string, LatencyStats> stats_;
};

/**
 * RAII helper for automatic section profiling.
 */
class ScopedProfile {
public:
    explicit ScopedProfile(const std::string& name) 
        : name_(name) {
        Profiler::getInstance().startSection(name_);
    }

    ~ScopedProfile() {
        Profiler::getInstance().endSection(name_);
    }

private:
    std::string name_;
};

// Convenience macros
#define PROFILE_SECTION(name) \
    trading::utils::ScopedProfile __profile_##__LINE__(name)

#define PROFILE_START(name) \
    trading::utils::Profiler::getInstance().startSection(name)

#define PROFILE_END(name) \
    trading::utils::Profiler::getInstance().endSection(name)

} // namespace utils
} // namespace trading

#endif // PROFILER_HPP