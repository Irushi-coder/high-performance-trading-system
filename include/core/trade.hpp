#ifndef TRADE_HPP
#define TRADE_HPP

#include "core/types.hpp"
#include <chrono>
#include <string>

namespace trading {

/**
 * Trade represents an executed trade between two orders.
 */
class Trade {
public:
    Trade(OrderId buyOrderId, OrderId sellOrderId, 
          const Symbol& symbol, Price price, Quantity quantity)
        : buyOrderId_(buyOrderId)
        , sellOrderId_(sellOrderId)
        , symbol_(symbol)
        , price_(price)
        , quantity_(quantity)
        , timestamp_(getCurrentTimestamp())
    {}

    // Getters
    OrderId getBuyOrderId() const { return buyOrderId_; }
    OrderId getSellOrderId() const { return sellOrderId_; }
    const Symbol& getSymbol() const { return symbol_; }
    Price getPrice() const { return price_; }
    Quantity getQuantity() const { return quantity_; }
    Timestamp getTimestamp() const { return timestamp_; }

    // Calculate trade value
    double getValue() const {
        return priceToDouble(price_) * quantity_;
    }

    // Check if order was involved in this trade
    bool involvesOrder(OrderId orderId) const {
        return buyOrderId_ == orderId || sellOrderId_ == orderId;
    }

    // Get the aggressor side (the order that triggered the trade)
    // Convention: Market orders are always aggressors
    // For limit orders, the incoming order is the aggressor
    OrderId getAggressorOrderId(bool buyWasAggressor) const {
        return buyWasAggressor ? buyOrderId_ : sellOrderId_;
    }

    // String representation
    std::string toString() const {
        return "Trade[buy=" + std::to_string(buyOrderId_) +
               ", sell=" + std::to_string(sellOrderId_) +
               ", symbol=" + symbol_ +
               ", price=" + std::to_string(priceToDouble(price_)) +
               ", qty=" + std::to_string(quantity_) +
               ", value=$" + std::to_string(getValue()) + "]";
    }

    // CSV format for logging
    std::string toCSV() const {
        return std::to_string(timestamp_) + "," +
               std::to_string(buyOrderId_) + "," +
               std::to_string(sellOrderId_) + "," +
               symbol_ + "," +
               std::to_string(priceToDouble(price_)) + "," +
               std::to_string(quantity_) + "," +
               std::to_string(getValue());
    }

private:
    OrderId buyOrderId_;
    OrderId sellOrderId_;
    Symbol symbol_;
    Price price_;
    Quantity quantity_;
    Timestamp timestamp_;

    static Timestamp getCurrentTimestamp() {
        auto now = std::chrono::high_resolution_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()
        );
        return static_cast<Timestamp>(nanos.count());
    }
};

} // namespace trading

#endif // TRADE_HPP