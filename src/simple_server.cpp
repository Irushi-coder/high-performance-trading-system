#include "engine/matching_engine.hpp"
#include "network/tcp_server.hpp"
#include "network/fix_message.hpp"
#include "network/market_data.hpp"
#include "utils/logger.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace trading;
using namespace trading::network;
using namespace trading::utils;

int main() {
    Logger::getInstance().setLogLevel(LogLevel::INFO);
    
    LOG_INFO("========================================");
    LOG_INFO("Starting Trading Server on port 8080");
    LOG_INFO("========================================");
    
    TCPServer server(8080);
    MatchingEngine engine("AAPL");
    
    // Handle incoming messages
    server.setMessageCallback([&](const std::string& message, socket_t client) {
        LOG_INFO("Received: ", message);
        
        try {
            FIXMessage fixMsg = FIXMessage::parse(message);
            auto order = fixMsg.toOrder();
            
            if (order) {
                LOG_INFO("Processing: ", order->toString());
                auto trades = engine.submitOrder(order);
                
                // Send execution report
                FIXMessage execReport = FIXMessage::createExecutionReport(
                    *order, "EXEC_" + std::to_string(order->getId())
                );
                server.sendMessage(client, execReport.serialize());
                
                // Broadcast order book update
                std::string bookUpdate = MarketDataPublisher::formatOrderBookJSON(
                    engine.getOrderBook()
                );
                server.broadcast(bookUpdate + "\n");
            }
        } catch (...) {
            LOG_ERROR("Error processing message");
        }
    });
    
    // Handle trades
    engine.setTradeCallback([&](const Trade& trade) {
        LOG_INFO("TRADE: ", trade.toString());
        std::string tradeMsg = MarketDataPublisher::formatTrade(trade);
        server.broadcast(tradeMsg + "\n");
    });
    
    if (server.start()) {
        LOG_INFO("✓ Server started successfully!");
        LOG_INFO("Connect using: telnet localhost 8080");
        LOG_INFO("Press Ctrl+C to stop...\n");
        
        // Keep running
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Print stats every 10 seconds
            static int counter = 0;
            if (++counter % 10 == 0) {
                auto stats = engine.getStats();
                LOG_INFO("Stats - Clients: ", server.getClientCount(),
                        ", Trades: ", stats.totalTrades,
                        ", Volume: ", stats.totalVolume);
            }
        }
    } else {
        LOG_ERROR("Failed to start server!");
    }
    
    return 0;
}