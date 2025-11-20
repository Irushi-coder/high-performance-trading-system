#ifndef RISK_MANAGER_HPP
#define RISK_MANAGER_HPP

#include "core/types.hpp"
#include "core/order.hpp"
#include "core/trade.hpp"
#include <unordered_map>
#include <string>
#include <cmath>

namespace trading {
namespace risk {

/**
 * Position represents a trader's position in a symbol.
 */
struct Position {
    Symbol symbol;
    int64_t quantity;           // Positive = long, negative = short
    double averagePrice;
    double realizedPnL;
    double unrealizedPnL;
    Quantity totalBought;
    Quantity totalSold;

    Position() 
        : quantity(0)
        , averagePrice(0.0)
        , realizedPnL(0.0)
        , unrealizedPnL(0.0)
        , totalBought(0)
        , totalSold(0)
    {}

    Position(const Symbol& sym)
        : symbol(sym)
        , quantity(0)
        , averagePrice(0.0)
        , realizedPnL(0.0)
        , unrealizedPnL(0.0)
        , totalBought(0)
        , totalSold(0)
    {}

    bool isFlat() const { return quantity == 0; }
    bool isLong() const { return quantity > 0; }
    bool isShort() const { return quantity < 0; }

    double getMarketValue(double currentPrice) const {
        return std::abs(quantity) * currentPrice;
    }

    void updateUnrealizedPnL(double currentPrice) {
        if (quantity == 0) {
            unrealizedPnL = 0.0;
        } else {
            unrealizedPnL = quantity * (currentPrice - averagePrice);
        }
    }
};

/**
 * RiskLimits defines trading constraints.
 */
struct RiskLimits {
    Quantity maxOrderSize;          // Max single order quantity
    double maxOrderValue;            // Max single order value ($)
    int64_t maxPositionSize;        // Max position (long or short)
    double maxPositionValue;         // Max position value ($)
    double maxDailyLoss;             // Max loss per day ($)
    double maxDrawdown;              // Max drawdown from peak ($)
    int maxOrdersPerSecond;          // Rate limit

    RiskLimits()
        : maxOrderSize(10000)
        , maxOrderValue(1000000.0)
        , maxPositionSize(50000)
        , maxPositionValue(5000000.0)
        , maxDailyLoss(100000.0)
        , maxDrawdown(200000.0)
        , maxOrdersPerSecond(100)
    {}
};

/**
 * RiskManager enforces trading limits and tracks positions.
 */
class RiskManager {
public:
    explicit RiskManager(const RiskLimits& limits = RiskLimits())
        : limits_(limits)
        , dailyPnL_(0.0)
        , peakEquity_(0.0)
        , currentEquity_(0.0)
    {}

    /**
     * Validate order before submission.
     */
    enum class ValidationResult {
        ACCEPTED,
        REJECTED_ORDER_SIZE,
        REJECTED_ORDER_VALUE,
        REJECTED_POSITION_LIMIT,
        REJECTED_POSITION_VALUE,
        REJECTED_DAILY_LOSS,
        REJECTED_DRAWDOWN,
        REJECTED_RATE_LIMIT
    };

    ValidationResult validateOrder(const Order& order, double currentPrice = 0.0) {
        // Check order size
        if (order.getQuantity() > limits_.maxOrderSize) {
            return ValidationResult::REJECTED_ORDER_SIZE;
        }

        // Check order value
        double orderPrice = (order.getType() == OrderType::MARKET) ? 
                           currentPrice : priceToDouble(order.getPrice());
        double orderValue = order.getQuantity() * orderPrice;
        
        if (orderValue > limits_.maxOrderValue) {
            return ValidationResult::REJECTED_ORDER_VALUE;
        }

        // Check position limits
        auto& position = positions_[order.getSymbol()];
        int64_t newQuantity = position.quantity;
        
        if (order.getSide() == Side::BUY) {
            newQuantity += order.getQuantity();
        } else {
            newQuantity -= order.getQuantity();
        }

        if (std::abs(newQuantity) > limits_.maxPositionSize) {
            return ValidationResult::REJECTED_POSITION_LIMIT;
        }

        double newPositionValue = std::abs(newQuantity) * orderPrice;
        if (newPositionValue > limits_.maxPositionValue) {
            return ValidationResult::REJECTED_POSITION_VALUE;
        }

        // Check daily loss limit
        if (dailyPnL_ < -limits_.maxDailyLoss) {
            return ValidationResult::REJECTED_DAILY_LOSS;
        }

        // Check drawdown
        double drawdown = peakEquity_ - currentEquity_;
        if (drawdown > limits_.maxDrawdown) {
            return ValidationResult::REJECTED_DRAWDOWN;
        }

        return ValidationResult::ACCEPTED;
    }

    /**
     * Update position after trade execution.
     */
    void updatePosition(const Trade& trade, Side aggressorSide) {
        const Symbol& symbol = trade.getSymbol();
        auto& position = positions_[symbol];

        if (position.symbol.empty()) {
            position.symbol = symbol;
        }

        double tradePrice = priceToDouble(trade.getPrice());
        Quantity tradeQty = trade.getQuantity();

        // Determine if this is a buy or sell for position tracking
        bool isBuy = (aggressorSide == Side::BUY);

        if (isBuy) {
            position.totalBought += tradeQty;
            
            if (position.quantity >= 0) {
                // Adding to long or opening long
                position.averagePrice = 
                    (position.quantity * position.averagePrice + tradeQty * tradePrice) /
                    (position.quantity + tradeQty);
                position.quantity += tradeQty;
            } else {
                // Covering short
                int64_t closingQty = std::min(static_cast<int64_t>(tradeQty), 
                                              std::abs(position.quantity));
                double pnl = closingQty * (position.averagePrice - tradePrice);
                position.realizedPnL += pnl;
                dailyPnL_ += pnl;
                
                position.quantity += tradeQty;
                
                if (position.quantity > 0) {
                    // Flipped to long
                    position.averagePrice = tradePrice;
                }
            }
        } else {
            position.totalSold += tradeQty;
            
            if (position.quantity <= 0) {
                // Adding to short or opening short
                position.averagePrice = 
                    (std::abs(position.quantity) * position.averagePrice + tradeQty * tradePrice) /
                    (std::abs(position.quantity) + tradeQty);
                position.quantity -= tradeQty;
            } else {
                // Closing long
                int64_t closingQty = std::min(static_cast<int64_t>(tradeQty), position.quantity);
                double pnl = closingQty * (tradePrice - position.averagePrice);
                position.realizedPnL += pnl;
                dailyPnL_ += pnl;
                
                position.quantity -= tradeQty;
                
                if (position.quantity < 0) {
                    // Flipped to short
                    position.averagePrice = tradePrice;
                }
            }
        }

        // Update equity
        currentEquity_ = dailyPnL_;
        for (auto& [sym, pos] : positions_) {
            currentEquity_ += pos.unrealizedPnL;
        }
        
        if (currentEquity_ > peakEquity_) {
            peakEquity_ = currentEquity_;
        }
    }

    /**
     * Update unrealized P&L for all positions.
     */
    void updateUnrealizedPnL(const Symbol& symbol, double currentPrice) {
        auto it = positions_.find(symbol);
        if (it != positions_.end()) {
            it->second.updateUnrealizedPnL(currentPrice);
        }
    }

    /**
     * Get position for a symbol.
     */
    const Position& getPosition(const Symbol& symbol) const {
        static Position empty;
        auto it = positions_.find(symbol);
        return (it != positions_.end()) ? it->second : empty;
    }

    /**
     * Get all positions.
     */
    const std::unordered_map<Symbol, Position>& getAllPositions() const {
        return positions_;
    }

    /**
     * Get total P&L.
     */
    double getTotalPnL() const {
        double total = dailyPnL_;
        for (const auto& [symbol, position] : positions_) {
            total += position.unrealizedPnL;
        }
        return total;
    }

    /**
     * Get daily P&L.
     */
    double getDailyPnL() const { return dailyPnL_; }

    /**
     * Get current drawdown.
     */
    double getCurrentDrawdown() const {
        return peakEquity_ - currentEquity_;
    }

    /**
     * Reset daily statistics.
     */
    void resetDaily() {
        dailyPnL_ = 0.0;
        for (auto& [symbol, position] : positions_) {
            position.realizedPnL = 0.0;
        }
    }

    /**
     * Get risk limits.
     */
    const RiskLimits& getLimits() const { return limits_; }

    /**
     * Set risk limits.
     */
    void setLimits(const RiskLimits& limits) { limits_ = limits; }

    /**
     * String representation of validation result.
     */
    static const char* validationResultToString(ValidationResult result) {
        switch (result) {
            case ValidationResult::ACCEPTED: return "ACCEPTED";
            case ValidationResult::REJECTED_ORDER_SIZE: return "REJECTED: Order size too large";
            case ValidationResult::REJECTED_ORDER_VALUE: return "REJECTED: Order value too large";
            case ValidationResult::REJECTED_POSITION_LIMIT: return "REJECTED: Position limit exceeded";
            case ValidationResult::REJECTED_POSITION_VALUE: return "REJECTED: Position value too large";
            case ValidationResult::REJECTED_DAILY_LOSS: return "REJECTED: Daily loss limit exceeded";
            case ValidationResult::REJECTED_DRAWDOWN: return "REJECTED: Drawdown limit exceeded";
            case ValidationResult::REJECTED_RATE_LIMIT: return "REJECTED: Rate limit exceeded";
            default: return "UNKNOWN";
        }
    }

private:
    RiskLimits limits_;
    std::unordered_map<Symbol, Position> positions_;
    double dailyPnL_;
    double peakEquity_;
    double currentEquity_;
};

} // namespace risk
} // namespace trading

#endif // RISK_MANAGER_HPP