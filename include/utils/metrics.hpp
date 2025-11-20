#ifndef METRICS_HPP
#define METRICS_HPP

#include <atomic>
#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>

namespace trading {
namespace utils {

/**
 * SystemMetrics tracks key performance indicators.
 */
class SystemMetrics {
public:
    static SystemMetrics& getInstance() {
        static SystemMetrics instance;
        return instance;
    }

    // Order metrics
    void recordOrderSubmitted() { ordersSubmitted_.fetch_add(1, std::memory_order_relaxed); }
    void recordOrderAccepted() { ordersAccepted_.fetch_add(1, std::memory_order_relaxed); }
    void recordOrderRejected() { ordersRejected_.fetch_add(1, std::memory_order_relaxed); }
    void recordOrderCancelled() { ordersCancelled_.fetch_add(1, std::memory_order_relaxed); }

    // Trade metrics
    void recordTrade(uint64_t volume, double value) {
        tradesExecuted_.fetch_add(1, std::memory_order_relaxed);
        volumeTraded_.fetch_add(volume, std::memory_order_relaxed);
        valueTraded_.fetch_add(static_cast<uint64_t>(value * 100), std::memory_order_relaxed);
    }

    // Latency metrics
    void recordLatency(uint64_t latencyNs) {
        totalLatency_.fetch_add(latencyNs, std::memory_order_relaxed);
        latencyMeasurements_.fetch_add(1, std::memory_order_relaxed);
    }

    // Error metrics
    void recordError() { errors_.fetch_add(1, std::memory_order_relaxed); }
    void recordWarning() { warnings_.fetch_add(1, std::memory_order_relaxed); }

    // Connection metrics
    void recordConnectionEstablished() { 
        connections_.fetch_add(1, std::memory_order_relaxed); 
    }
    void recordConnectionClosed() { 
        connections_.fetch_sub(1, std::memory_order_relaxed); 
    }

    // Getters
    uint64_t getOrdersSubmitted() const { return ordersSubmitted_.load(); }
    uint64_t getOrdersAccepted() const { return ordersAccepted_.load(); }
    uint64_t getOrdersRejected() const { return ordersRejected_.load(); }
    uint64_t getOrdersCancelled() const { return ordersCancelled_.load(); }
    uint64_t getTradesExecuted() const { return tradesExecuted_.load(); }
    uint64_t getVolumeTraded() const { return volumeTraded_.load(); }
    double getValueTraded() const { return valueTraded_.load() / 100.0; }
    uint64_t getErrors() const { return errors_.load(); }
    uint64_t getWarnings() const { return warnings_.load(); }
    int64_t getActiveConnections() const { return connections_.load(); }

    double getAverageLatency() const {
        uint64_t measurements = latencyMeasurements_.load();
        if (measurements == 0) return 0.0;
        return static_cast<double>(totalLatency_.load()) / measurements;
    }

    // Statistics
    struct Stats {
        uint64_t ordersSubmitted;
        uint64_t ordersAccepted;
        uint64_t ordersRejected;
        uint64_t ordersCancelled;
        uint64_t tradesExecuted;
        uint64_t volumeTraded;
        double valueTraded;
        double averageLatency;
        uint64_t errors;
        uint64_t warnings;
        int64_t activeConnections;
        uint64_t uptimeSeconds;
    };

    Stats getStats() const {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            now - startTime_
        ).count();

        return {
            ordersSubmitted_.load(),
            ordersAccepted_.load(),
            ordersRejected_.load(),
            ordersCancelled_.load(),
            tradesExecuted_.load(),
            volumeTraded_.load(),
            getValueTraded(),
            getAverageLatency(),
            errors_.load(),
            warnings_.load(),
            connections_.load(),
            static_cast<uint64_t>(uptime)
        };
    }

    // Reset all metrics
    void reset() {
        ordersSubmitted_ = 0;
        ordersAccepted_ = 0;
        ordersRejected_ = 0;
        ordersCancelled_ = 0;
        tradesExecuted_ = 0;
        volumeTraded_ = 0;
        valueTraded_ = 0;
        totalLatency_ = 0;
        latencyMeasurements_ = 0;
        errors_ = 0;
        warnings_ = 0;
        startTime_ = std::chrono::steady_clock::now();
    }

    // Format as string
    std::string toString() const {
        auto stats = getStats();
        std::ostringstream oss;
        
        oss << "\n========== SYSTEM METRICS ==========\n";
        oss << "Uptime:              " << formatUptime(stats.uptimeSeconds) << "\n";
        oss << "\nOrders:\n";
        oss << "  Submitted:         " << stats.ordersSubmitted << "\n";
        oss << "  Accepted:          " << stats.ordersAccepted << "\n";
        oss << "  Rejected:          " << stats.ordersRejected << "\n";
        oss << "  Cancelled:         " << stats.ordersCancelled << "\n";
        
        double acceptRate = (stats.ordersSubmitted > 0) ? 
            (100.0 * stats.ordersAccepted / stats.ordersSubmitted) : 0.0;
        oss << "  Accept Rate:       " << std::fixed << std::setprecision(1) 
            << acceptRate << "%\n";
        
        oss << "\nTrades:\n";
        oss << "  Executed:          " << stats.tradesExecuted << "\n";
        oss << "  Volume:            " << stats.volumeTraded << " shares\n";
        oss << "  Value:             $" << std::fixed << std::setprecision(2) 
            << stats.valueTraded << "\n";
        
        oss << "\nPerformance:\n";
        oss << "  Avg Latency:       " << std::fixed << std::setprecision(2) 
            << (stats.averageLatency / 1000.0) << " µs\n";
        
        if (stats.uptimeSeconds > 0) {
            double ordersPerSec = static_cast<double>(stats.ordersSubmitted) / stats.uptimeSeconds;
            double tradesPerSec = static_cast<double>(stats.tradesExecuted) / stats.uptimeSeconds;
            oss << "  Orders/sec:        " << std::fixed << std::setprecision(1) 
                << ordersPerSec << "\n";
            oss << "  Trades/sec:        " << std::fixed << std::setprecision(1) 
                << tradesPerSec << "\n";
        }
        
        oss << "\nConnections:\n";
        oss << "  Active:            " << stats.activeConnections << "\n";
        
        oss << "\nErrors:\n";
        oss << "  Errors:            " << stats.errors << "\n";
        oss << "  Warnings:          " << stats.warnings << "\n";
        
        oss << "====================================\n";
        return oss.str();
    }

private:
    SystemMetrics() 
        : startTime_(std::chrono::steady_clock::now())
        , ordersSubmitted_(0)
        , ordersAccepted_(0)
        , ordersRejected_(0)
        , ordersCancelled_(0)
        , tradesExecuted_(0)
        , volumeTraded_(0)
        , valueTraded_(0)
        , totalLatency_(0)
        , latencyMeasurements_(0)
        , errors_(0)
        , warnings_(0)
        , connections_(0)
    {}

    std::chrono::steady_clock::time_point startTime_;
    
    std::atomic<uint64_t> ordersSubmitted_;
    std::atomic<uint64_t> ordersAccepted_;
    std::atomic<uint64_t> ordersRejected_;
    std::atomic<uint64_t> ordersCancelled_;
    std::atomic<uint64_t> tradesExecuted_;
    std::atomic<uint64_t> volumeTraded_;
    std::atomic<uint64_t> valueTraded_;  // In cents
    std::atomic<uint64_t> totalLatency_;
    std::atomic<uint64_t> latencyMeasurements_;
    std::atomic<uint64_t> errors_;
    std::atomic<uint64_t> warnings_;
    std::atomic<int64_t> connections_;

    static std::string formatUptime(uint64_t seconds) {
        uint64_t days = seconds / 86400;
        uint64_t hours = (seconds % 86400) / 3600;
        uint64_t mins = (seconds % 3600) / 60;
        uint64_t secs = seconds % 60;
        
        std::ostringstream oss;
        if (days > 0) oss << days << "d ";
        if (hours > 0) oss << hours << "h ";
        if (mins > 0) oss << mins << "m ";
        oss << secs << "s";
        
        return oss.str();
    }
};

} // namespace utils
} // namespace trading

#endif // METRICS_HPP