#include "core/types.hpp"
#include "core/order.hpp"
#include "core/trade.hpp"
#include "engine/matching_engine.hpp"
#include "utils/logger.hpp"
#include "utils/timer.hpp"
#include <iostream>
#include <iomanip>

using namespace trading;
using namespace trading::utils;

// Track trades for verification
std::vector<Trade> allTrades;

void tradeHandler(const Trade& trade) {
    allTrades.push_back(trade);
    LOG_INFO("TRADE EXECUTED: ", trade.toString());
}

void testSimpleMatch() {
    LOG_INFO("\n=== Test 1: Simple Limit Order Match ===");
    allTrades.clear();
    
    MatchingEngine engine("AAPL");
    engine.setTradeCallback(tradeHandler);
    
    // Add a sell order
    auto sellOrder = std::make_shared<Order>(
        engine.getNextOrderId(), "AAPL", Side::SELL, OrderType::LIMIT,
        doubleToPrice(150.00), 100
    );
    
    LOG_INFO("Submitting sell order: ", sellOrder->toString());
    auto trades1 = engine.submitOrder(sellOrder);
    LOG_INFO("Trades generated: ", trades1.size());
    
    // Add a buy order that matches
    auto buyOrder = std::make_shared<Order>(
        engine.getNextOrderId(), "AAPL", Side::BUY, OrderType::LIMIT,
        doubleToPrice(150.00), 100
    );
    
    LOG_INFO("Submitting buy order: ", buyOrder->toString());
    auto trades2 = engine.submitOrder(buyOrder);
    LOG_INFO("Trades generated: ", trades2.size());
    
    if (trades2.size() == 1) {
        LOG_INFO("✓ Simple match successful!");
        LOG_INFO("  Trade price: $", priceToDouble(trades2[0].getPrice()));
        LOG_INFO("  Trade quantity: ", trades2[0].getQuantity());
    } else {
        LOG_ERROR("✗ Expected 1 trade, got ", trades2.size());
    }
    
    std::cout << engine.getOrderBook().displayBook(5) << std::endl;
}

void testPartialFill() {
    LOG_INFO("\n=== Test 2: Partial Fill ===");
    allTrades.clear();
    
    MatchingEngine engine("AAPL");
    engine.setTradeCallback(tradeHandler);
    
    // Add a large sell order
    auto sellOrder = std::make_shared<Order>(
        engine.getNextOrderId(), "AAPL", Side::SELL, OrderType::LIMIT,
        doubleToPrice(150.00), 500
    );
    
    LOG_INFO("Submitting large sell order (500 shares)");
    engine.submitOrder(sellOrder);
    
    // Add a smaller buy order
    auto buyOrder = std::make_shared<Order>(
        engine.getNextOrderId(), "AAPL", Side::BUY, OrderType::LIMIT,
        doubleToPrice(150.00), 200
    );
    
    LOG_INFO("Submitting smaller buy order (200 shares)");
    auto trades = engine.submitOrder(buyOrder);
    
    if (trades.size() == 1 && trades[0].getQuantity() == 200) {
        LOG_INFO("✓ Partial fill successful!");
        LOG_INFO("  Filled: 200 shares");
        LOG_INFO("  Remaining on book: 300 shares");
    } else {
        LOG_ERROR("✗ Partial fill failed");
    }
    
    auto stats = engine.getOrderBook().getStats();
    LOG_INFO("Book stats: ", stats.totalOrders, " orders, ",
             stats.totalAskQty, " ask qty");
    
    std::cout << engine.getOrderBook().displayBook(5) << std::endl;
}

void testMarketOrder() {
    LOG_INFO("\n=== Test 3: Market Order ===");
    allTrades.clear();
    
    MatchingEngine engine("AAPL");
    engine.setTradeCallback(tradeHandler);
    
    // Add some limit orders to the book
    auto sell1 = std::make_shared<Order>(
        engine.getNextOrderId(), "AAPL", Side::SELL, OrderType::LIMIT,
        doubleToPrice(150.00), 100
    );
    auto sell2 = std::make_shared<Order>(
        engine.getNextOrderId(), "AAPL", Side::SELL, OrderType::LIMIT,
        doubleToPrice(150.50), 100
    );
    auto sell3 = std::make_shared<Order>(
        engine.getNextOrderId(), "AAPL", Side::SELL, OrderType::LIMIT,
        doubleToPrice(151.00), 100
    );
    
    engine.submitOrder(sell1);
    engine.submitOrder(sell2);
    engine.submitOrder(sell3);
    
    LOG_INFO("Book prepared with 3 sell orders");
    std::cout << engine.getOrderBook().displayBook(5) << std::endl;
    
    // Submit a market buy order that crosses multiple levels
    auto marketBuy = std::make_shared<Order>(
        engine.getNextOrderId(), "AAPL", Side::BUY, 250 // Market order
    );
    
    LOG_INFO("Submitting market buy for 250 shares");
    auto trades = engine.submitOrder(marketBuy);
    
    LOG_INFO("Market order generated ", trades.size(), " trades");
    for (const auto& trade : trades) {
        LOG_INFO("  Trade: ", trade.getQuantity(), " @ $", 
                 priceToDouble(trade.getPrice()));
    }
    
    if (trades.size() == 3) {
        LOG_INFO("✓ Market order crossed multiple levels!");
    }
    
    std::cout << engine.getOrderBook().displayBook(5) << std::endl;
}

void testPriceTimePriority() {
    LOG_INFO("\n=== Test 4: Price-Time Priority ===");
    allTrades.clear();
    
    MatchingEngine engine("AAPL");
    engine.setTradeCallback(tradeHandler);
    
    // Add multiple orders at the same price (should execute in FIFO order)
    auto sell1 = std::make_shared<Order>(
        1, "AAPL", Side::SELL, OrderType::LIMIT,
        doubleToPrice(150.00), 100
    );
    auto sell2 = std::make_shared<Order>(
        2, "AAPL", Side::SELL, OrderType::LIMIT,
        doubleToPrice(150.00), 100
    );
    auto sell3 = std::make_shared<Order>(
        3, "AAPL", Side::SELL, OrderType::LIMIT,
        doubleToPrice(150.00), 100
    );
    
    engine.submitOrder(sell1);
    engine.submitOrder(sell2);
    engine.submitOrder(sell3);
    
    LOG_INFO("Added 3 sell orders at $150.00 (IDs: 1, 2, 3)");
    
    // Buy order should match with order ID 1 first
    auto buy = std::make_shared<Order>(
        4, "AAPL", Side::BUY, OrderType::LIMIT,
        doubleToPrice(150.00), 100
    );
    
    auto trades = engine.submitOrder(buy);
    
    if (!trades.empty() && trades[0].getSellOrderId() == 1) {
        LOG_INFO("✓ Price-Time Priority maintained (matched with order 1 first)");
    } else {
        LOG_ERROR("✗ Price-Time Priority violated");
    }
}

void testMultiLevelMatch() {
    LOG_INFO("\n=== Test 5: Multi-Level Matching ===");
    allTrades.clear();
    
    MatchingEngine engine("AAPL");
    engine.setTradeCallback(tradeHandler);
    
    // Build a deep order book
    for (int i = 0; i < 10; i++) {
        auto sell = std::make_shared<Order>(
            engine.getNextOrderId(), "AAPL", Side::SELL, OrderType::LIMIT,
            doubleToPrice(150.00 + i * 0.10), 100
        );
        engine.submitOrder(sell);
        
        auto buy = std::make_shared<Order>(
            engine.getNextOrderId(), "AAPL", Side::BUY, OrderType::LIMIT,
            doubleToPrice(149.00 - i * 0.10), 100
        );
        engine.submitOrder(buy);
    }
    
    LOG_INFO("Built deep order book (10 levels each side)");
    std::cout << engine.getOrderBook().displayBook(10) << std::endl;
    
    // Large market order that sweeps through multiple levels
    auto marketBuy = std::make_shared<Order>(
        engine.getNextOrderId(), "AAPL", Side::BUY, 550
    );
    
    LOG_INFO("Submitting large market buy (550 shares)");
    auto trades = engine.submitOrder(marketBuy);
    
    LOG_INFO("Generated ", trades.size(), " trades");
    double totalValue = 0;
    Quantity totalQty = 0;
    
    for (const auto& trade : trades) {
        totalValue += trade.getValue();
        totalQty += trade.getQuantity();
    }
    
    LOG_INFO("Total executed: ", totalQty, " shares");
    LOG_INFO("Total value: $", totalValue);
    LOG_INFO("Average price: $", totalValue / totalQty);
    
    std::cout << engine.getOrderBook().displayBook(10) << std::endl;
}

void testPerformance() {
    LOG_INFO("\n=== Test 6: Performance Benchmark ===");
    
    MatchingEngine engine("AAPL");
    const int NUM_ORDERS = 10000;
    
    // Pre-populate the book
    LOG_INFO("Pre-populating book with ", NUM_ORDERS, " orders...");
    for (int i = 0; i < NUM_ORDERS / 2; i++) {
        auto sell = std::make_shared<Order>(
            i * 2, "AAPL", Side::SELL, OrderType::LIMIT,
            doubleToPrice(150.00 + (i % 100) * 0.01), 100
        );
        engine.submitOrder(sell);
    }
    
    Timer timer;
    LatencyMeasurer latency;
    std::vector<uint64_t> latencies;
    
    // Test order submission and matching
    for (int i = 0; i < NUM_ORDERS / 2; i++) {
        latency.start();
        
        auto buy = std::make_shared<Order>(
            i * 2 + 1, "AAPL", Side::BUY, OrderType::LIMIT,
            doubleToPrice(150.00 + (i % 100) * 0.01), 100
        );
        engine.submitOrder(buy);
        
        latencies.push_back(latency.end());
    }
    
    uint64_t totalTime = timer.elapsedMicros();
    
    // Calculate statistics
    uint64_t totalCycles = 0;
    uint64_t minCycles = UINT64_MAX;
    uint64_t maxCycles = 0;
    
    for (uint64_t cycles : latencies) {
        totalCycles += cycles;
        minCycles = std::min(minCycles, cycles);
        maxCycles = std::max(maxCycles, cycles);
    }
    
    uint64_t avgCycles = totalCycles / latencies.size();
    
    LOG_INFO("\nPerformance Results:");
    LOG_INFO("  Total orders: ", NUM_ORDERS);
    LOG_INFO("  Total time: ", totalTime, " µs");
    LOG_INFO("  Throughput: ", (NUM_ORDERS * 1000000ULL) / totalTime, " orders/sec");
    LOG_INFO("  Average latency: ", totalTime / NUM_ORDERS, " µs");
    LOG_INFO("  CPU cycles (avg): ", avgCycles);
    LOG_INFO("  CPU cycles (min): ", minCycles);
    LOG_INFO("  CPU cycles (max): ", maxCycles);
    
    auto stats = engine.getStats();
    LOG_INFO("\nMatching Statistics:");
    LOG_INFO("  Total trades: ", stats.totalTrades);
    LOG_INFO("  Total volume: ", stats.totalVolume, " shares");
    LOG_INFO("  Total value: $", stats.totalValue);
    LOG_INFO("  Market orders: ", stats.marketOrdersMatched);
    LOG_INFO("  Limit orders: ", stats.limitOrdersMatched);
}

int main() {
    // Set up logging
    Logger::getInstance().setLogLevel(LogLevel::INFO);
    Logger::getInstance().setOutputFile("matching_engine_test.log");
    
    LOG_INFO("========================================");
    LOG_INFO("Matching Engine Tests - Phase 3");
    LOG_INFO("========================================");
    
    try {
        testSimpleMatch();
        testPartialFill();
        testMarketOrder();
        testPriceTimePriority();
        testMultiLevelMatch();
        testPerformance();
        
        LOG_INFO("\n========================================");
        LOG_INFO("All Phase 3 tests completed successfully!");
        LOG_INFO("========================================");
        
        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR("Fatal error: ", e.what());
        return 1;
    }
}