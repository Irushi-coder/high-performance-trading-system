#include "core/types.hpp"
#include "core/order.hpp"
#include "core/trade.hpp"
#include "engine/matching_engine.hpp"
#include "network/fix_message.hpp"
#include "network/tcp_server.hpp"
#include "network/market_data.hpp"
#include "utils/logger.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace trading;
using namespace trading::network;
using namespace trading::utils;

void testFIXMessageParsing() {
    LOG_INFO("\n=== Test 1: FIX Message Parsing ===");
    
    // Create a new order FIX message
    FIXMessage newOrder = FIXMessage::createNewOrder(
        12345,              // Client order ID
        "AAPL",            // Symbol
        Side::BUY,         // Side
        OrderType::LIMIT,  // Type
        100,               // Quantity
        doubleToPrice(150.50)  // Price
    );
    
    LOG_INFO("Created FIX new order:");
    LOG_INFO(newOrder.toString());
    
    // Serialize to FIX protocol format
    std::string serialized = newOrder.serialize();
    LOG_INFO("\nSerialized FIX message (", serialized.length(), " bytes)");
    
    // Parse back
    FIXMessage parsed = FIXMessage::parse(serialized);
    LOG_INFO("Parsed message:");
    LOG_INFO(parsed.toString());
    
    // Convert to Order object
    auto order = parsed.toOrder();
    if (order) {
        LOG_INFO("\nConverted to Order:");
        LOG_INFO(order->toString());
        LOG_INFO("✓ FIX parsing successful");
    } else {
        LOG_ERROR("✗ Failed to convert FIX to Order");
    }
}

void testFIXExecutionReport() {
    LOG_INFO("\n=== Test 2: FIX Execution Report ===");
    
    Order order(1, "AAPL", Side::BUY, OrderType::LIMIT, 
                doubleToPrice(150.00), 100);
    
    // Simulate partial fill
    order.fillQuantity(30);
    
    // Create execution report
    FIXMessage execReport = FIXMessage::createExecutionReport(
        order,
        "EXEC123",  // Execution ID
        '1',        // Partial fill
        30,         // Last quantity
        doubleToPrice(150.00)  // Last price
    );
    
    LOG_INFO("Execution Report:");
    LOG_INFO(execReport.toString());
    
    std::string serialized = execReport.serialize();
    LOG_INFO("Serialized (", serialized.length(), " bytes)");
    
    LOG_INFO("✓ Execution report test completed");
}

void testMarketDataFormatting() {
    LOG_INFO("\n=== Test 3: Market Data Formatting ===");
    
    // Build a sample order book
    OrderBook book("AAPL");
    
    for (int i = 0; i < 5; ++i) {
        auto buy = std::make_shared<Order>(
            i * 2, "AAPL", Side::BUY, OrderType::LIMIT,
            doubleToPrice(150.00 - i * 0.10), 100 + i * 20
        );
        book.addOrder(buy);
        
        auto sell = std::make_shared<Order>(
            i * 2 + 1, "AAPL", Side::SELL, OrderType::LIMIT,
            doubleToPrice(151.00 + i * 0.10), 100 + i * 20
        );
        book.addOrder(sell);
    }
    
    // Format as JSON
    std::string json = MarketDataPublisher::formatOrderBookSnapshot(book);
    LOG_INFO("Order Book JSON:");
    std::cout << json << std::endl;
    
    // Format as text
    std::string text = MarketDataPublisher::formatOrderBookText(book);
    LOG_INFO("\nOrder Book Text:");
    std::cout << text << std::endl;
    
    // Format trade
    Trade trade(100, 101, "AAPL", doubleToPrice(150.50), 50);
    std::string tradeJson = MarketDataPublisher::formatTrade(trade);
    LOG_INFO("Trade JSON:");
    std::cout << tradeJson << std::endl;
    
    LOG_INFO("✓ Market data formatting test completed");
}

void testTCPServer() {
    LOG_INFO("\n=== Test 4: TCP Server ===");
    
    // Create server on port 9090
    TCPServer server(9090);
    MatchingEngine engine("AAPL");
    
    // Set up message handler
    server.setMessageCallback([&](const std::string& message, socket_t client) {
        LOG_INFO("Received message from client: ", message);
        
        // Try to parse as FIX message
        try {
            FIXMessage fixMsg = FIXMessage::parse(message);
            LOG_INFO("Parsed FIX message: ", fixMsg.toString());
            
            // Convert to order and submit
            auto order = fixMsg.toOrder();
            if (order) {
                LOG_INFO("Processing order: ", order->toString());
                auto trades = engine.submitOrder(order);
                
                // Send execution report back
                FIXMessage execReport = FIXMessage::createExecutionReport(
                    *order, "EXEC_" + std::to_string(order->getId())
                );
                
                std::string response = execReport.serialize();
                server.sendMessage(client, response);
                
                // Broadcast trades
                for (const auto& trade : trades) {
                    std::string tradeMsg = MarketDataPublisher::formatTrade(trade);
                    server.broadcast(tradeMsg);
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error processing message: ", e.what());
        }
    });
    
    // Start server
    if (server.start()) {
        LOG_INFO("✓ TCP Server started on port 9090");
        LOG_INFO("Waiting for connections...");
        LOG_INFO("You can connect using: telnet localhost 9090");
        LOG_INFO("Or use a FIX client to send orders");
        
        // Run for a short time (in real application, would run indefinitely)
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        LOG_INFO("Connected clients: ", server.getClientCount());
        
        server.stop();
        LOG_INFO("✓ TCP Server stopped");
    } else {
        LOG_ERROR("✗ Failed to start TCP server");
    }
}

void testIntegratedSystem() {
    LOG_INFO("\n=== Test 5: Integrated Trading System ===");
    
    // Create components
    TCPServer server(9091);
    MatchingEngine engine("AAPL");
    
    std::atomic<int> tradeCount{0};
    
    // Set up trade callback
    engine.setTradeCallback([&](const Trade& trade) {
        tradeCount++;
        LOG_INFO("TRADE EXECUTED: ", trade.toString());
        
        // Broadcast to all clients
        std::string tradeMsg = MarketDataPublisher::formatTrade(trade);
        server.broadcast(tradeMsg);
    });
    
    // Set up FIX message handler
    server.setMessageCallback([&](const std::string& message, socket_t client) {
        try {
            FIXMessage fixMsg = FIXMessage::parse(message);
            auto order = fixMsg.toOrder();
            
            if (order) {
                // Submit order to engine
                auto trades = engine.submitOrder(order);
                
                // Send execution report
                char execType = '0';  // New
                if (order->getStatus() == OrderStatus::FILLED) {
                    execType = '2';  // Filled
                } else if (order->getStatus() == OrderStatus::PARTIALLY_FILLED) {
                    execType = '1';  // Partial
                }
                
                FIXMessage execReport = FIXMessage::createExecutionReport(
                    *order, 
                    "EXEC_" + std::to_string(order->getId()),
                    execType
                );
                
                server.sendMessage(client, execReport.serialize());
                
                // Broadcast updated order book
                std::string bookUpdate = MarketDataPublisher::formatOrderBookSnapshot(
                    engine.getOrderBook()
                );
                server.broadcast(bookUpdate);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error: ", e.what());
        }
    });
    
    // Start server
    if (server.start()) {
        LOG_INFO("✓ Integrated system started on port 9091");
        
        // Simulate some orders from internal source
        std::thread simulator([&]() {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            for (int i = 0; i < 10; ++i) {
                auto order = std::make_shared<Order>(
                    1000 + i, "AAPL",
                    (i % 2 == 0) ? Side::BUY : Side::SELL,
                    OrderType::LIMIT,
                    doubleToPrice(150.0 + (i % 5) * 0.10),
                    100
                );
                
                engine.submitOrder(order);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
        
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        simulator.join();
        
        LOG_INFO("\nFinal Statistics:");
        LOG_INFO("  Trades executed: ", tradeCount.load());
        LOG_INFO("  Connected clients: ", server.getClientCount());
        
        auto stats = engine.getStats();
        LOG_INFO("  Total trades: ", stats.totalTrades);
        LOG_INFO("  Total volume: ", stats.totalVolume);
        
        server.stop();
        LOG_INFO("✓ Integrated system test completed");
    }
}

int main() {
    Logger::getInstance().setLogLevel(LogLevel::INFO);
    Logger::getInstance().setOutputFile("network_test.log");
    
    LOG_INFO("========================================");
    LOG_INFO("Network & FIX Protocol Tests - Phase 5");
    LOG_INFO("========================================");
    
    try {
        testFIXMessageParsing();
        testFIXExecutionReport();
        testMarketDataFormatting();
        testTCPServer();
        testIntegratedSystem();
        
        LOG_INFO("\n========================================");
        LOG_INFO("All Phase 5 tests completed successfully!");
        LOG_INFO("========================================");
        
        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR("Fatal error: ", e.what());
        return 1;
    }
}