#include "core/types.hpp"
#include "core/order.hpp"
#include "engine/matching_engine.hpp"
#include "utils/memory_pool.hpp"
#include "utils/lockfree_queue.hpp"
#include "utils/profiler.hpp"
#include "utils/logger.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace trading;
using namespace trading::utils;

void testMemoryPool() {
    LOG_INFO("\n=== Test 1: Memory Pool Performance ===");
    
    MemoryPool<Order, 1024> pool;
    const int NUM_ALLOCS = 100000;
    
    // Test allocation speed
    Timer timer;
    std::vector<Order*> orders;
    orders.reserve(NUM_ALLOCS);
    
    for (int i = 0; i < NUM_ALLOCS; ++i) {
        Order* order = pool.construct(
            i, "AAPL", Side::BUY, OrderType::LIMIT,
            doubleToPrice(150.0), 100
        );
        orders.push_back(order);
    }
    
    uint64_t allocTime = timer.elapsedMicros();
    
    LOG_INFO("Allocated ", NUM_ALLOCS, " orders in ", allocTime, " µs");
    LOG_INFO("Average: ", allocTime * 1000.0 / NUM_ALLOCS, " ns per allocation");
    LOG_INFO("Rate: ", (NUM_ALLOCS * 1000000ULL) / allocTime, " allocations/sec");
    
    // Test deallocation speed
    timer.reset();
    for (Order* order : orders) {
        pool.destroy(order);
    }
    
    uint64_t deallocTime = timer.elapsedMicros();
    LOG_INFO("Deallocated ", NUM_ALLOCS, " orders in ", deallocTime, " µs");
    LOG_INFO("Average: ", deallocTime * 1000.0 / NUM_ALLOCS, " ns per deallocation");
    
    auto stats = pool.getStats();
    LOG_INFO("Pool stats: ", stats.blocksAllocated, " blocks, ",
             stats.totalCapacity, " capacity");
    
    LOG_INFO("✓ Memory pool test completed");
}

void testLockFreeQueue() {
    LOG_INFO("\n=== Test 2: Lock-Free Queue Performance ===");
    
    LockFreeQueue<int, 4096> queue;
    const int NUM_OPS = 1000000;
    
    // Test push performance
    Timer timer;
    for (int i = 0; i < NUM_OPS; ++i) {
        queue.tryPush(i);
    }
    uint64_t pushTime = timer.elapsedMicros();
    
    LOG_INFO("Pushed ", NUM_OPS, " items in ", pushTime, " µs");
    LOG_INFO("Average: ", pushTime * 1000.0 / NUM_OPS, " ns per push");
    LOG_INFO("Rate: ", (NUM_OPS * 1000000ULL) / pushTime, " pushes/sec");
    
    // Test pop performance
    timer.reset();
    int value;
    int poppedCount = 0;
    while (queue.tryPop(value)) {
        poppedCount++;
    }
    uint64_t popTime = timer.elapsedMicros();
    
    LOG_INFO("Popped ", poppedCount, " items in ", popTime, " µs");
    LOG_INFO("Average: ", popTime * 1000.0 / poppedCount, " ns per pop");
    
    LOG_INFO("✓ Lock-free queue test completed");
}

void testOrderLatency() {
    LOG_INFO("\n=== Test 3: Order Processing Latency ===");
    
    MatchingEngine engine("AAPL");
    Profiler& profiler = Profiler::getInstance();
    profiler.clear();
    
    const int NUM_ORDERS = 10000;
    LatencyMeasurer latency;
    
    // Pre-populate book
    for (int i = 0; i < NUM_ORDERS / 2; ++i) {
        auto sell = std::make_shared<Order>(
            i, "AAPL", Side::SELL, OrderType::LIMIT,
            doubleToPrice(150.0 + (i % 50) * 0.01), 100
        );
        engine.submitOrder(sell);
    }
    
    LOG_INFO("Book pre-populated with ", NUM_ORDERS / 2, " sell orders");
    
    // Measure order submission latency
    std::vector<uint64_t> latencies;
    latencies.reserve(NUM_ORDERS / 2);
    
    for (int i = 0; i < NUM_ORDERS / 2; ++i) {
        auto buy = std::make_shared<Order>(
            NUM_ORDERS / 2 + i, "AAPL", Side::BUY, OrderType::LIMIT,
            doubleToPrice(150.0 + (i % 50) * 0.01), 100
        );
        
        latency.start();
        engine.submitOrder(buy);
        uint64_t cycles = latency.end();
        
        latencies.push_back(cycles);
        profiler.recordCycles("OrderSubmission", cycles);
    }
    
    // Calculate statistics
    std::sort(latencies.begin(), latencies.end());
    
    uint64_t sum = 0;
    for (uint64_t l : latencies) {
        sum += l;
    }
    
    uint64_t avg = sum / latencies.size();
    uint64_t p50 = latencies[latencies.size() / 2];
    uint64_t p95 = latencies[(latencies.size() * 95) / 100];
    uint64_t p99 = latencies[(latencies.size() * 99) / 100];
    uint64_t p999 = latencies[(latencies.size() * 999) / 1000];
    
    LOG_INFO("\nLatency Statistics (CPU cycles):");
    LOG_INFO("  Average: ", avg);
    LOG_INFO("  Min: ", latencies.front());
    LOG_INFO("  Max: ", latencies.back());
    LOG_INFO("  P50: ", p50);
    LOG_INFO("  P95: ", p95);
    LOG_INFO("  P99: ", p99);
    LOG_INFO("  P99.9: ", p999);
    
    // Estimate nanoseconds (assuming 2.5 GHz CPU)
    double nsPerCycle = 1.0 / 2.5;
    LOG_INFO("\nEstimated Latency (nanoseconds @ 2.5 GHz):");
    LOG_INFO("  Average: ", static_cast<uint64_t>(avg * nsPerCycle), " ns");
    LOG_INFO("  P50: ", static_cast<uint64_t>(p50 * nsPerCycle), " ns");
    LOG_INFO("  P99: ", static_cast<uint64_t>(p99 * nsPerCycle), " ns");
    
    LOG_INFO("✓ Order latency test completed");
}

void testThroughput() {
    LOG_INFO("\n=== Test 4: System Throughput ===");
    
    MatchingEngine engine("AAPL");
    const int NUM_ORDERS = 100000;
    
    Timer timer;
    
    // Mixed order types
    for (int i = 0; i < NUM_ORDERS; ++i) {
        Side side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        
        auto order = std::make_shared<Order>(
            i, "AAPL", side, OrderType::LIMIT,
            doubleToPrice(150.0 + (i % 100) * 0.01), 100
        );
        
        engine.submitOrder(order);
    }
    
    uint64_t elapsed = timer.elapsedMicros();
    
    LOG_INFO("Processed ", NUM_ORDERS, " orders in ", elapsed, " µs");
    LOG_INFO("Average: ", elapsed * 1000.0 / NUM_ORDERS, " ns per order");
    LOG_INFO("Throughput: ", (NUM_ORDERS * 1000000ULL) / elapsed, " orders/sec");
    
    auto stats = engine.getStats();
    LOG_INFO("\nMatching Statistics:");
    LOG_INFO("  Total trades: ", stats.totalTrades);
    LOG_INFO("  Total volume: ", stats.totalVolume, " shares");
    LOG_INFO("  Total value: $", static_cast<uint64_t>(stats.totalValue));
    
    LOG_INFO("✓ Throughput test completed");
}

void testCacheBehavior() {
    LOG_INFO("\n=== Test 5: Cache Behavior Analysis ===");
    
    const int NUM_ITERATIONS = 1000000;
    
    // Test 1: Sequential access (cache-friendly)
    std::vector<int> sequential(1000);
    Timer timer;
    
    volatile int sum = 0;
    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        for (size_t i = 0; i < sequential.size(); ++i) {
            sum += sequential[i];
        }
    }
    
    uint64_t seqTime = timer.elapsedMicros();
    LOG_INFO("Sequential access: ", seqTime, " µs");
    
    // Test 2: Random access (cache-unfriendly)
    std::vector<int> indices(1000);
    for (size_t i = 0; i < indices.size(); ++i) {
        indices[i] = (i * 37) % 1000;  // Pseudo-random
    }
    
    timer.reset();
    sum = 0;
    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        for (size_t i = 0; i < indices.size(); ++i) {
            sum += sequential[indices[i]];
        }
    }
    
    uint64_t randTime = timer.elapsedMicros();
    LOG_INFO("Random access: ", randTime, " µs");
    LOG_INFO("Random/Sequential ratio: ", 
             static_cast<double>(randTime) / seqTime, "x slower");
    
    LOG_INFO("✓ Cache behavior test completed");
}

void testMultithreadedSubmission() {
    LOG_INFO("\n=== Test 6: Multi-threaded Order Submission ===");
    
    MatchingEngine engine("AAPL");
    const int NUM_THREADS = 4;
    const int ORDERS_PER_THREAD = 25000;
    
    std::vector<std::thread> threads;
    std::atomic<uint64_t> totalLatency{0};
    
    Timer timer;
    
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&engine, t, ORDERS_PER_THREAD, &totalLatency]() {
            LatencyMeasurer latency;
            uint64_t threadLatency = 0;
            
            for (int i = 0; i < ORDERS_PER_THREAD; ++i) {
                Side side = (i % 2 == 0) ? Side::BUY : Side::SELL;
                
                auto order = std::make_shared<Order>(
                    t * ORDERS_PER_THREAD + i,
                    "AAPL", side, OrderType::LIMIT,
                    doubleToPrice(150.0 + (i % 50) * 0.01), 100
                );
                
                latency.start();
                engine.submitOrder(order);
                threadLatency += latency.end();
            }
            
            totalLatency.fetch_add(threadLatency, std::memory_order_relaxed);
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    uint64_t elapsed = timer.elapsedMicros();
    int totalOrders = NUM_THREADS * ORDERS_PER_THREAD;
    
    LOG_INFO("Processed ", totalOrders, " orders from ", NUM_THREADS, 
             " threads in ", elapsed, " µs");
    LOG_INFO("Throughput: ", (totalOrders * 1000000ULL) / elapsed, " orders/sec");
    
    uint64_t avgCycles = totalLatency.load() / totalOrders;
    LOG_INFO("Average latency: ", avgCycles, " cycles");
    LOG_INFO("Estimated: ", static_cast<uint64_t>(avgCycles / 2.5), " ns @ 2.5 GHz");
    
    LOG_INFO("✓ Multi-threaded test completed");
}

int main() {
    Logger::getInstance().setLogLevel(LogLevel::INFO);
    Logger::getInstance().setOutputFile("performance_test.log");
    
    LOG_INFO("========================================");
    LOG_INFO("Performance Optimization Tests - Phase 4");
    LOG_INFO("========================================");
    
    try {
        testMemoryPool();
        testLockFreeQueue();
        testOrderLatency();
        testThroughput();
        testCacheBehavior();
        testMultithreadedSubmission();
        
        LOG_INFO("\n========================================");
        LOG_INFO("All Phase 4 tests completed successfully!");
        LOG_INFO("========================================");
        
        // Print profiler report
        std::cout << Profiler::getInstance().getReport() << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR("Fatal error: ", e.what());
        return 1;
    }
}