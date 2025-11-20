#include "engine/matching_engine.hpp"
#include "network/websocket_server.hpp"
#include "network/market_data.hpp"
#include "risk/risk_manager.hpp"
#include "utils/metrics.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>

using namespace trading;
using namespace trading::network;
using namespace trading::risk;
using namespace trading::utils;

std::string createMetricsJSON(const SystemMetrics::Stats& stats) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "{"
        << "\"type\":\"metrics\","
        << "\"ordersSubmitted\":" << stats.ordersSubmitted << ","
        << "\"tradesExecuted\":" << stats.tradesExecuted << ","
        << "\"avgLatency\":" << (stats.averageLatency / 1000.0) << ","
        << "\"throughput\":" << (stats.ordersSubmitted / (stats.uptimeSeconds + 1))
        << "}";
    return oss.str();
}

std::string createOrderBookJSON(const OrderBook& book) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    
    oss << "{\"type\":\"orderbook\",";
    
    // Bids
    auto bids = book.getBidDepth(5);
    oss << "\"bids\":[";
    for (size_t i = 0; i < bids.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "{\"price\":" << priceToDouble(bids[i].price)
            << ",\"quantity\":" << bids[i].quantity << "}";
    }
    oss << "],";
    
    // Asks
    auto asks = book.getAskDepth(5);
    oss << "\"asks\":[";
    for (size_t i = 0; i < asks.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "{\"price\":" << priceToDouble(asks[i].price)
            << ",\"quantity\":" << asks[i].quantity << "}";
    }
    oss << "],";
    
    // Spread
    auto spread = book.getSpread();
    oss << "\"spread\":" << (spread ? priceToDouble(*spread) : 0.0);
    
    oss << "}";
    return oss.str();
}

std::string createTradeJSON(const Trade& trade) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "{"
        << "\"type\":\"trade\","
        << "\"timestamp\":" << trade.getTimestamp() << ","
        << "\"price\":" << priceToDouble(trade.getPrice()) << ","
        << "\"quantity\":" << trade.getQuantity()
        << "}";
    return oss.str();
}

std::string createRiskJSON(const RiskManager& riskMgr, const Position& pos) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "{"
        << "\"type\":\"risk\","
        << "\"position\":" << pos.quantity << ","
        << "\"dailyPnL\":" << riskMgr.getDailyPnL() << ","
        << "\"ordersRejected\":" << SystemMetrics::getInstance().getOrdersRejected() << ","
        << "\"connections\":0"
        << "}";
    return oss.str();
}

int main() {
    Logger::getInstance().setLogLevel(LogLevel::INFO);
    
    LOG_INFO("========================================");
    LOG_INFO("Trading System Dashboard Server");
    LOG_INFO("========================================\n");
    
    // Load configuration
    Config& config = Config::getInstance();
    config.loadFromFile("trading_config.txt");
    
    int wsPort = config.getInt("dashboard.port", 8080);
    
    // Create components
    WebSocketServer wsServer(wsPort);
    MatchingEngine engine("AAPL");
    
    RiskLimits limits;
    limits.maxOrderSize = config.getInt("risk.max_order_size", 10000);
    limits.maxPositionSize = config.getInt("risk.max_position_size", 50000);
    RiskManager riskMgr(limits);
    
    SystemMetrics& metrics = SystemMetrics::getInstance();
    metrics.reset();
    
    // Set up trade callback
    engine.setTradeCallback([&](const Trade& trade) {
        LOG_INFO("TRADE: ", trade.toString());
        metrics.recordTrade(trade.getQuantity(), trade.getValue());
        riskMgr.updatePosition(trade, Side::BUY);
        
        // Broadcast to dashboard
        wsServer.broadcast(createTradeJSON(trade));
    });
    
    // Start WebSocket server
    if (!wsServer.start()) {
        LOG_ERROR("Failed to start WebSocket server on port ", wsPort);
        return 1;
    }
    
    LOG_INFO("✓ WebSocket server started on port ", wsPort);
    LOG_INFO("✓ Open dashboard.html in your browser");
    LOG_INFO("✓ Or navigate to http://localhost:", wsPort, "\n");
    
    // Background thread for periodic updates
    std::atomic<bool> running{true};
    std::thread updateThread([&]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Broadcast metrics
            auto stats = metrics.getStats();
            wsServer.broadcast(createMetricsJSON(stats));
            
            // Broadcast order book
            wsServer.broadcast(createOrderBookJSON(engine.getOrderBook()));
            
            // Broadcast risk info
            const Position& pos = riskMgr.getPosition("AAPL");
            wsServer.broadcast(createRiskJSON(riskMgr, pos));
        }
    });
    
    // Simulation thread - generates random orders
    std::thread simulationThread([&]() {
        OrderId nextOrderId = 1;
        
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Generate random order
            Side side = (rand() % 2 == 0) ? Side::BUY : Side::SELL;
            Price basePrice = doubleToPrice(150.0);
            Price priceOffset = doubleToPrice((rand() % 100) * 0.01);
            Price orderPrice = side == Side::BUY ? 
                              basePrice - priceOffset : basePrice + priceOffset;
            Quantity qty = (rand() % 300) + 100;
            
            auto order = std::make_shared<Order>(
                nextOrderId++, "AAPL", side, OrderType::LIMIT,
                orderPrice, qty
            );
            
            metrics.recordOrderSubmitted();
            
            // Risk check
            auto result = riskMgr.validateOrder(*order, priceToDouble(orderPrice));
            if (result == RiskManager::ValidationResult::ACCEPTED) {
                metrics.recordOrderAccepted();
                engine.submitOrder(order);
            } else {
                metrics.recordOrderRejected();
                LOG_WARN("Order ", order->getId(), " rejected: ",
                        RiskManager::validationResultToString(result));
            }
        }
    });
    
    // Print stats periodically
    LOG_INFO("System running. Press Ctrl+C to stop.\n");
    LOG_INFO("Dashboard clients: Connect to ws://localhost:", wsPort, "\n");
    
    int counter = 0;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        if (++counter % 10 == 0) {
            auto stats = metrics.getStats();
            LOG_INFO("Stats - Orders: ", stats.ordersSubmitted,
                    ", Trades: ", stats.tradesExecuted,
                    ", Clients: ", wsServer.getClientCount());
            
            if (counter % 60 == 0) {
                std::cout << metrics.toString() << std::endl;
            }
        }
    }
    
    running = false;
    updateThread.join();
    simulationThread.join();
    
    return 0;
}