#include "core/types.hpp"
#include "core/order.hpp"
#include "utils/logger.hpp"
#include "utils/timer.hpp"
#include <iostream>
#include <vector>

using namespace trading;
using namespace trading::utils;

void testBasicOrders() {
    LOG_INFO("=== Testing Basic Order Creation ===");
    
    // Create some sample orders
    Order buyOrder1(1, "AAPL", Side::BUY, OrderType::LIMIT, 
                    doubleToPrice(150.50), 100);
    Order sellOrder1(2, "AAPL", Side::SELL, OrderType::LIMIT, 
                     doubleToPrice(151.00), 50);
    Order marketOrder(3, "AAPL", Side::BUY, 75);

    LOG_INFO(buyOrder1.toString());
    LOG_INFO(sellOrder1.toString());
    LOG_INFO(marketOrder.toString());

    // Test order matching logic
    if (buyOrder1.canMatch(sellOrder1)) {
        LOG_INFO("Buy order can match with sell order");
    } else {
        LOG_INFO("Buy order cannot match with sell order (price mismatch)");
    }

    // Simulate partial fill
    LOG_INFO("\n=== Testing Partial Fill ===");
    buyOrder1.fillQuantity(30);
    LOG_INFO("After filling 30 units: ", buyOrder1.toString());
    
    buyOrder1.fillQuantity(70);
    LOG_INFO("After filling 70 more units: ", buyOrder1.toString());
}

void testPerformance() {
    LOG_INFO("\n=== Testing Performance ===");
    
    const int NUM_ORDERS = 100000;
    std::vector<Order> orders;
    orders.reserve(NUM_ORDERS);

    Timer timer;
    
    // Test order creation speed
    for (int i = 0; i < NUM_ORDERS; ++i) {
        orders.emplace_back(
            i, 
            "AAPL", 
            (i % 2 == 0) ? Side::BUY : Side::SELL,
            OrderType::LIMIT,
            doubleToPrice(150.0 + (i % 100) * 0.01),
            100
        );
    }
    
    uint64_t elapsed = timer.elapsedMicros();
    double ordersPerSecond = (NUM_ORDERS * 1000000.0) / elapsed;
    
    LOG_INFO("Created ", NUM_ORDERS, " orders in ", elapsed, " µs");
    LOG_INFO("Rate: ", static_cast<uint64_t>(ordersPerSecond), " orders/second");
    LOG_INFO("Average: ", elapsed / NUM_ORDERS, " µs per order");
}

void testLatencyMeasurement() {
    LOG_INFO("\n=== Testing Latency Measurement ===");
    
    LatencyMeasurer latency;
    
    // Measure order creation latency
    const int ITERATIONS = 1000;
    uint64_t totalCycles = 0;
    
    for (int i = 0; i < ITERATIONS; ++i) {
        latency.start();
        Order order(i, "AAPL", Side::BUY, OrderType::LIMIT, 
                    doubleToPrice(150.0), 100);
        totalCycles += latency.end();
    }
    
    uint64_t avgCycles = totalCycles / ITERATIONS;
    double avgNanos = latency.cyclesToNanos(avgCycles);
    
    LOG_INFO("Average order creation latency:");
    LOG_INFO("  Cycles: ", avgCycles);
    LOG_INFO("  Estimated time: ", static_cast<uint64_t>(avgNanos), " ns");
}

int main() {
    // Set up logging
    Logger::getInstance().setLogLevel(LogLevel::INFO);
    Logger::getInstance().setOutputFile("trading_system.log");
    
    LOG_INFO("========================================");
    LOG_INFO("High-Performance Trading System - Phase 1");
    LOG_INFO("========================================\n");
    
    try {
        // Run tests
        testBasicOrders();
        testPerformance();
        testLatencyMeasurement();
        
        LOG_INFO("\n========================================");
        LOG_INFO("All Phase 1 tests completed successfully!");
        LOG_INFO("========================================");
        
        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR("Fatal error: ", e.what());
        return 1;
    }
}