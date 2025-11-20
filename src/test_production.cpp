#include "core/types.hpp"
#include "core/order.hpp"
#include "core/trade.hpp"
#include "engine/matching_engine.hpp"
#include "risk/risk_manager.hpp"
#include "utils/config.hpp"
#include "utils/metrics.hpp"
#include "utils/logger.hpp"
#include <iostream>
#include <fstream>

using namespace trading;
using namespace trading::risk;
using namespace trading::utils;

void testConfiguration() {
    LOG_INFO("\n=== Test 1: Configuration System ===");
    
    // Create sample config file
    std::ofstream configFile("trading_config.txt");
    configFile << "# Trading System Configuration\n";
    configFile << "server.port=8080\n";
    configFile << "server.max_clients=100\n";
    configFile << "risk.max_order_size=10000\n";
    configFile << "risk.max_position_size=50000\n";
    configFile << "risk.max_daily_loss=100000.00\n";
    configFile << "logging.level=INFO\n";
    configFile << "logging.file=trading.log\n";
    configFile << "matching.enable_profiling=true\n";
    configFile.close();
    
    // Load configuration
    Config& config = Config::getInstance();
    if (config.loadFromFile("trading_config.txt")) {
        LOG_INFO("✓ Configuration loaded successfully");
        
        // Read values
        int port = config.getInt("server.port");
        int maxClients = config.getInt("server.max_clients");
        int maxOrderSize = config.getInt("risk.max_order_size");
        double maxDailyLoss = config.getDouble("risk.max_daily_loss");
        bool enableProfiling = config.getBool("matching.enable_profiling");
        
        LOG_INFO("  Port: ", port);
        LOG_INFO("  Max Clients: ", maxClients);
        LOG_INFO("  Max Order Size: ", maxOrderSize);
        LOG_INFO("  Max Daily Loss: $", maxDailyLoss);
        LOG_INFO("  Profiling: ", (enableProfiling ? "enabled" : "disabled"));
        
        config.print();
    } else {
        LOG_ERROR("✗ Failed to load configuration");
    }
}

void testRiskManagement() {
    LOG_INFO("\n=== Test 2: Risk Management ===");
    
    // Set up risk limits
    RiskLimits limits;
    limits.maxOrderSize = 1000;
    limits.maxOrderValue = 150000.0;
    limits.maxPositionSize = 5000;
    limits.maxDailyLoss = 50000.0;
    
    RiskManager riskMgr(limits);
    
    // Test 1: Valid order
    Order validOrder(1, "AAPL", Side::BUY, OrderType::LIMIT, 
                     doubleToPrice(150.00), 500);
    auto result = riskMgr.validateOrder(validOrder, 150.00);
    LOG_INFO("Valid order (500 shares): ", 
             RiskManager::validationResultToString(result));
    
    // Test 2: Order too large
    Order tooLarge(2, "AAPL", Side::BUY, OrderType::LIMIT,
                   doubleToPrice(150.00), 2000);
    result = riskMgr.validateOrder(tooLarge, 150.00);
    LOG_INFO("Too large order (2000 shares): ",
             RiskManager::validationResultToString(result));
    
    // Test 3: Position tracking
    LOG_INFO("\n--- Position Tracking ---");
    
    Trade trade1(1, 100, "AAPL", doubleToPrice(150.00), 300);
    riskMgr.updatePosition(trade1, Side::BUY);
    
    const Position& pos = riskMgr.getPosition("AAPL");
    LOG_INFO("After buying 300 shares:");
    LOG_INFO("  Position: ", pos.quantity, " shares");
    LOG_INFO("  Avg Price: $", pos.averagePrice);
    LOG_INFO("  Realized P&L: $", pos.realizedPnL);
    
    // Sell some
    Trade trade2(2, 101, "AAPL", doubleToPrice(152.00), 100);
    riskMgr.updatePosition(trade2, Side::SELL);
    
    const Position& pos2 = riskMgr.getPosition("AAPL");
    LOG_INFO("\nAfter selling 100 shares at $152:");
    LOG_INFO("  Position: ", pos2.quantity, " shares");
    LOG_INFO("  Realized P&L: $", pos2.realizedPnL);
    LOG_INFO("  Daily P&L: $", riskMgr.getDailyPnL());
    
    // Test 4: Position limit
    Order wouldExceedLimit(3, "AAPL", Side::BUY, OrderType::LIMIT,
                           doubleToPrice(150.00), 5000);
    result = riskMgr.validateOrder(wouldExceedLimit, 150.00);
    LOG_INFO("\nOrder that would exceed position limit:");
    LOG_INFO("  ", RiskManager::validationResultToString(result));
    
    LOG_INFO("\n✓ Risk management tests completed");
}

void testMetrics() {
    LOG_INFO("\n=== Test 3: System Metrics ===");
    
    SystemMetrics& metrics = SystemMetrics::getInstance();
    metrics.reset();
    
    // Simulate activity
    for (int i = 0; i < 100; ++i) {
        metrics.recordOrderSubmitted();
        
        if (i % 10 == 0) {
            metrics.recordOrderRejected();
        } else {
            metrics.recordOrderAccepted();
        }
    }
    
    // Record some trades
    for (int i = 0; i < 50; ++i) {
        metrics.recordTrade(100, 15000.0);
        metrics.recordLatency(1500);  // 1.5 microseconds
    }
    
    // Record some errors
    metrics.recordError();
    metrics.recordError();
    metrics.recordWarning();
    
    // Print metrics
    std::cout << metrics.toString() << std::endl;
    
    LOG_INFO("✓ Metrics test completed");
}

void testIntegratedSystem() {
    LOG_INFO("\n=== Test 4: Integrated System with Risk & Metrics ===");
    
    // Set up components
    MatchingEngine engine("AAPL");
    
    RiskLimits limits;
    limits.maxOrderSize = 1000;
    limits.maxPositionSize = 5000;
    RiskManager riskMgr(limits);
    
    SystemMetrics& metrics = SystemMetrics::getInstance();
    metrics.reset();
    
    // Track trades
    engine.setTradeCallback([&](const Trade& trade) {
        LOG_INFO("TRADE: ", trade.toString());
        metrics.recordTrade(trade.getQuantity(), trade.getValue());
        
        // Update positions (determine aggressor)
        riskMgr.updatePosition(trade, Side::BUY);  // Simplified
    });
    
    // Submit orders with risk checks
    auto submitWithRiskCheck = [&](std::shared_ptr<Order> order) {
        metrics.recordOrderSubmitted();
        
        // Validate
        auto result = riskMgr.validateOrder(*order, 
                                            priceToDouble(order->getPrice()));
        
        if (result != RiskManager::ValidationResult::ACCEPTED) {
            LOG_WARN("Order rejected: ", 
                    RiskManager::validationResultToString(result));
            metrics.recordOrderRejected();
            return;
        }
        
        metrics.recordOrderAccepted();
        auto trades = engine.submitOrder(order);
        
        LOG_INFO("Order ", order->getId(), " submitted, generated ", 
                trades.size(), " trades");
    };
    
    // Test scenario
    LOG_INFO("\n--- Submitting Orders ---");
    
    // Valid orders
    auto order1 = std::make_shared<Order>(
        1, "AAPL", Side::SELL, OrderType::LIMIT,
        doubleToPrice(150.00), 500
    );
    submitWithRiskCheck(order1);
    
    auto order2 = std::make_shared<Order>(
        2, "AAPL", Side::BUY, OrderType::LIMIT,
        doubleToPrice(150.00), 300
    );
    submitWithRiskCheck(order2);
    
    // This should be rejected (too large)
    auto order3 = std::make_shared<Order>(
        3, "AAPL", Side::BUY, OrderType::LIMIT,
        doubleToPrice(150.00), 2000
    );
    submitWithRiskCheck(order3);
    
    // Print final state
    LOG_INFO("\n--- Final State ---");
    std::cout << metrics.toString() << std::endl;
    
    const Position& pos = riskMgr.getPosition("AAPL");
    LOG_INFO("\nFinal Position:");
    LOG_INFO("  Quantity: ", pos.quantity, " shares");
    LOG_INFO("  Avg Price: $", pos.averagePrice);
    LOG_INFO("  Realized P&L: $", pos.realizedPnL);
    LOG_INFO("  Total Bought: ", pos.totalBought);
    LOG_INFO("  Total Sold: ", pos.totalSold);
    
    LOG_INFO("\n✓ Integrated system test completed");
}

void testConfigurableSystem() {
    LOG_INFO("\n=== Test 5: Fully Configured Production System ===");
    
    // Load configuration
    Config& config = Config::getInstance();
    
    // Apply configuration to risk manager
    RiskLimits limits;
    limits.maxOrderSize = config.getInt("risk.max_order_size", 10000);
    limits.maxPositionSize = config.getInt("risk.max_position_size", 50000);
    limits.maxDailyLoss = config.getDouble("risk.max_daily_loss", 100000.0);
    
    LOG_INFO("Risk limits loaded from config:");
    LOG_INFO("  Max Order Size: ", limits.maxOrderSize);
    LOG_INFO("  Max Position Size: ", limits.maxPositionSize);
    LOG_INFO("  Max Daily Loss: $", limits.maxDailyLoss);
    
    RiskManager riskMgr(limits);
    MatchingEngine engine("AAPL");
    SystemMetrics& metrics = SystemMetrics::getInstance();
    
    // Run simulation
    LOG_INFO("\nRunning simulation with 100 orders...");
    
    int tradesExecuted = 0;
    engine.setTradeCallback([&](const Trade& trade) {
        tradesExecuted++;
        metrics.recordTrade(trade.getQuantity(), trade.getValue());
        riskMgr.updatePosition(trade, Side::BUY);
    });
    
    for (int i = 0; i < 100; ++i) {
        Side side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        
        auto order = std::make_shared<Order>(
            i, "AAPL", side, OrderType::LIMIT,
            doubleToPrice(150.0 + (i % 10) * 0.10),
            100
        );
        
        metrics.recordOrderSubmitted();
        
        auto result = riskMgr.validateOrder(*order, 150.0);
        if (result == RiskManager::ValidationResult::ACCEPTED) {
            metrics.recordOrderAccepted();
            engine.submitOrder(order);
        } else {
            metrics.recordOrderRejected();
        }
    }
    
    LOG_INFO("\nSimulation complete:");
    LOG_INFO("  Orders submitted: ", metrics.getOrdersSubmitted());
    LOG_INFO("  Orders accepted: ", metrics.getOrdersAccepted());
    LOG_INFO("  Orders rejected: ", metrics.getOrdersRejected());
    LOG_INFO("  Trades executed: ", tradesExecuted);
    LOG_INFO("  Daily P&L: $", riskMgr.getDailyPnL());
    
    LOG_INFO("\n✓ Configured system test completed");
}

int main() {
    Logger::getInstance().setLogLevel(LogLevel::INFO);
    Logger::getInstance().setOutputFile("production_test.log");
    
    LOG_INFO("========================================");
    LOG_INFO("Production Features Tests - Phase 6");
    LOG_INFO("========================================");
    
    try {
        testConfiguration();
        testRiskManagement();
        testMetrics();
        testIntegratedSystem();
        testConfigurableSystem();
        
        LOG_INFO("\n========================================");
        LOG_INFO("All Phase 6 tests completed successfully!");
        LOG_INFO("========================================");
        
        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR("Fatal error: ", e.what());
        return 1;
    }
}