#include "core/types.hpp"
#include "core/order.hpp"
#include "engine/price_level.hpp"
#include "engine/order_book.hpp"
#include "utils/logger.hpp"
#include "utils/timer.hpp"
#include <iostream>
#include <vector>

using namespace trading;
using namespace trading::utils;

void testPriceLevel() {
    LOG_INFO("=== Testing Price Level ===");
    
    PriceLevel level(doubleToPrice(150.00));
    
    // Add multiple orders at the same price
    auto order1 = std::make_shared<Order>(1, "AAPL", Side::BUY, OrderType::LIMIT, 
                                          doubleToPrice(150.00), 100);
    auto order2 = std::make_shared<Order>(2, "AAPL", Side::BUY, OrderType::LIMIT, 
                                          doubleToPrice(150.00), 200);
    auto order3 = std::make_shared<Order>(3, "AAPL", Side::BUY, OrderType::LIMIT, 
                                          doubleToPrice(150.00), 150);
    
    level.addOrder(order1);
    level.addOrder(order2);
    level.addOrder(order3);
    
    LOG_INFO("Added 3 orders: ", level.toString());
    LOG_INFO("Total quantity: ", level.getTotalQuantity());
    LOG_INFO("Order count: ", level.getOrderCount());
    
    // Test FIFO - first order should be at front
    auto frontOrder = level.getFrontOrder();
    LOG_INFO("Front order ID: ", frontOrder->getId(), " (should be 1)");
    
    // Remove middle order
    level.removeOrder(2);
    LOG_INFO("After removing order 2: ", level.toString());
    
    LOG_INFO("✓ Price Level tests passed\n");
}

void testOrderBookBasics() {
    LOG_INFO("=== Testing Order Book Basics ===");
    
    OrderBook book("AAPL");
    
    // Add buy orders
    auto buy1 = std::make_shared<Order>(1, "AAPL", Side::BUY, OrderType::LIMIT, 
                                        doubleToPrice(150.00), 100);
    auto buy2 = std::make_shared<Order>(2, "AAPL", Side::BUY, OrderType::LIMIT, 
                                        doubleToPrice(149.50), 200);
    auto buy3 = std::make_shared<Order>(3, "AAPL", Side::BUY, OrderType::LIMIT, 
                                        doubleToPrice(149.00), 150);
    
    // Add sell orders
    auto sell1 = std::make_shared<Order>(4, "AAPL", Side::SELL, OrderType::LIMIT, 
                                         doubleToPrice(151.00), 100);
    auto sell2 = std::make_shared<Order>(5, "AAPL", Side::SELL, OrderType::LIMIT, 
                                         doubleToPrice(151.50), 200);
    auto sell3 = std::make_shared<Order>(6, "AAPL", Side::SELL, OrderType::LIMIT, 
                                         doubleToPrice(152.00), 150);
    
    book.addOrder(buy1);
    book.addOrder(buy2);
    book.addOrder(buy3);
    book.addOrder(sell1);
    book.addOrder(sell2);
    book.addOrder(sell3);
    
    // Check best bid/ask
    auto bestBid = book.getBestBid();
    auto bestAsk = book.getBestAsk();
    
    LOG_INFO("Best Bid: $", priceToDouble(*bestBid));
    LOG_INFO("Best Ask: $", priceToDouble(*bestAsk));
    
    auto spread = book.getSpread();
    auto mid = book.getMidPrice();
    LOG_INFO("Spread: $", priceToDouble(*spread));
    LOG_INFO("Mid Price: $", *mid);
    
    // Get statistics
    auto stats = book.getStats();
    LOG_INFO("Total Orders: ", stats.totalOrders);
    LOG_INFO("Bid Levels: ", stats.bidLevels);
    LOG_INFO("Ask Levels: ", stats.askLevels);
    LOG_INFO("Total Bid Quantity: ", stats.totalBidQty);
    LOG_INFO("Total Ask Quantity: ", stats.totalAskQty);
    
    LOG_INFO("✓ Order Book basics tests passed\n");
}

void testOrderBookDisplay() {
    LOG_INFO("=== Testing Order Book Display ===");
    
    OrderBook book("AAPL");
    
    // Add multiple orders at different price levels
    for (int i = 0; i < 10; i++) {
        // Buy orders
        auto buyOrder = std::make_shared<Order>(
            i * 2,
            "AAPL",
            Side::BUY,
            OrderType::LIMIT,
            doubleToPrice(150.00 - i * 0.10),
            100 + i * 10
        );
        book.addOrder(buyOrder);
        
        // Sell orders
        auto sellOrder = std::make_shared<Order>(
            i * 2 + 1,
            "AAPL",
            Side::SELL,
            OrderType::LIMIT,
            doubleToPrice(151.00 + i * 0.10),
            100 + i * 10
        );
        book.addOrder(sellOrder);
    }
    
    // Display the book
    std::cout << book.displayBook(10) << std::endl;
    
    LOG_INFO("✓ Order Book display test passed\n");
}

void testOrderCancellation() {
    LOG_INFO("=== Testing Order Cancellation ===");
    
    OrderBook book("AAPL");
    
    auto order1 = std::make_shared<Order>(1, "AAPL", Side::BUY, OrderType::LIMIT, 
                                          doubleToPrice(150.00), 100);
    auto order2 = std::make_shared<Order>(2, "AAPL", Side::BUY, OrderType::LIMIT, 
                                          doubleToPrice(150.00), 200);
    
    book.addOrder(order1);
    book.addOrder(order2);
    
    LOG_INFO("Before cancel - Orders: ", book.getStats().totalOrders);
    LOG_INFO("Before cancel - Total Bid Qty: ", book.getTotalBidQuantity());
    
    // Cancel order 1
    bool cancelled = book.cancelOrder(1);
    LOG_INFO("Cancelled order 1: ", (cancelled ? "success" : "failed"));
    
    LOG_INFO("After cancel - Orders: ", book.getStats().totalOrders);
    LOG_INFO("After cancel - Total Bid Qty: ", book.getTotalBidQuantity());
    
    // Try to cancel non-existent order
    cancelled = book.cancelOrder(999);
    LOG_INFO("Cancelled non-existent order: ", (cancelled ? "success" : "failed (expected)"));
    
    LOG_INFO("✓ Order cancellation tests passed\n");
}

void testOrderModification() {
    LOG_INFO("=== Testing Order Modification ===");
    
    OrderBook book("AAPL");
    
    auto order = std::make_shared<Order>(1, "AAPL", Side::BUY, OrderType::LIMIT, 
                                         doubleToPrice(150.00), 100);
    book.addOrder(order);
    
    LOG_INFO("Original order: Price=$150.00, Qty=100");
    LOG_INFO("Best Bid: $", priceToDouble(*book.getBestBid()));
    
    // Modify the order
    bool modified = book.modifyOrder(1, doubleToPrice(151.00), 200);
    LOG_INFO("Modified order: ", (modified ? "success" : "failed"));
    
    LOG_INFO("New Best Bid: $", priceToDouble(*book.getBestBid()));
    auto modifiedOrder = book.getOrder(1);
    if (modifiedOrder) {
        LOG_INFO("Modified order details: Price=$", priceToDouble(modifiedOrder->getPrice()), 
                 ", Qty=", modifiedOrder->getQuantity());
    }
    
    LOG_INFO("✓ Order modification tests passed\n");
}

void testPerformance() {
    LOG_INFO("=== Testing Order Book Performance ===");
    
    OrderBook book("AAPL");
    const int NUM_ORDERS = 10000;
    
    Timer timer;
    
    // Test adding orders
    timer.reset();
    for (int i = 0; i < NUM_ORDERS; i++) {
        auto order = std::make_shared<Order>(
            i,
            "AAPL",
            (i % 2 == 0) ? Side::BUY : Side::SELL,
            OrderType::LIMIT,
            doubleToPrice(150.00 + (i % 100) * 0.01),
            100
        );
        book.addOrder(order);
    }
    uint64_t addTime = timer.elapsedMicros();
    
    LOG_INFO("Added ", NUM_ORDERS, " orders in ", addTime, " µs");
    LOG_INFO("Average: ", addTime / NUM_ORDERS, " µs per add");
    LOG_INFO("Rate: ", (NUM_ORDERS * 1000000ULL) / addTime, " adds/second");
    
    // Test order lookup
    timer.reset();
    for (int i = 0; i < NUM_ORDERS; i++) {
        auto order = book.getOrder(i);
    }
    uint64_t lookupTime = timer.elapsedMicros();
    
    LOG_INFO("Looked up ", NUM_ORDERS, " orders in ", lookupTime, " µs");
    LOG_INFO("Average: ", lookupTime / NUM_ORDERS, " µs per lookup");
    
    // Test best bid/ask access
    timer.reset();
    for (int i = 0; i < 1000000; i++) {
        auto bid = book.getBestBid();
        auto ask = book.getBestAsk();
    }
    uint64_t accessTime = timer.elapsedMicros();
    
    LOG_INFO("Accessed best bid/ask 1M times in ", accessTime, " µs");
    LOG_INFO("Average: ", accessTime / 1000000.0, " µs per access");
    
    // Display final book stats
    auto stats = book.getStats();
    LOG_INFO("\nFinal Book Statistics:");
    LOG_INFO("  Total Orders: ", stats.totalOrders);
    LOG_INFO("  Bid Levels: ", stats.bidLevels);
    LOG_INFO("  Ask Levels: ", stats.askLevels);
    LOG_INFO("  Total Bid Qty: ", stats.totalBidQty);
    LOG_INFO("  Total Ask Qty: ", stats.totalAskQty);
    
    LOG_INFO("✓ Performance tests completed\n");
}

int main() {
    // Set up logging
    Logger::getInstance().setLogLevel(LogLevel::INFO);
    Logger::getInstance().setOutputFile("order_book_test.log");
    
    LOG_INFO("========================================");
    LOG_INFO("Order Book Tests - Phase 2");
    LOG_INFO("========================================\n");
    
    try {
        testPriceLevel();
        testOrderBookBasics();
        testOrderBookDisplay();
        testOrderCancellation();
        testOrderModification();
        testPerformance();
        
        LOG_INFO("========================================");
        LOG_INFO("All Phase 2 tests completed successfully!");
        LOG_INFO("========================================");
        
        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR("Fatal error: ", e.what());
        return 1;
    }
}