#ifndef ORDER_HPP
#define ORDER_HPP

#include "core/types.hpp"
#include <chrono>
#include <string>

namespace trading {

class Order {
public:
    // Constructor for limit orders
    Order(OrderId id, Symbol symbol, Side side, OrderType type, 
          Price price, Quantity quantity)
        : id_(id)
        , symbol_(std::move(symbol))
        , side_(side)
        , type_(type)
        , price_(price)
        , quantity_(quantity)
        , remainingQuantity_(quantity)
        , status_(OrderStatus::NEW)
        , timestamp_(getCurrentTimestamp())
    {}

    // Constructor for market orders (no price)
    Order(OrderId id, Symbol symbol, Side side, Quantity quantity)
        : id_(id)
        , symbol_(std::move(symbol))
        , side_(side)
        , type_(OrderType::MARKET)
        , price_(0)
        , quantity_(quantity)
        , remainingQuantity_(quantity)
        , status_(OrderStatus::NEW)
        , timestamp_(getCurrentTimestamp())
    {}

    // Getters
    OrderId getId() const { return id_; }
    const Symbol& getSymbol() const { return symbol_; }
    Side getSide() const { return side_; }
    OrderType getType() const { return type_; }
    Price getPrice() const { return price_; }
    Quantity getQuantity() const { return quantity_; }
    Quantity getRemainingQuantity() const { return remainingQuantity_; }
    OrderStatus getStatus() const { return status_; }
    Timestamp getTimestamp() const { return timestamp_; }

    // Modifiers
    void setStatus(OrderStatus status) { status_ = status; }
    
    void fillQuantity(Quantity qty) {
        if (qty > remainingQuantity_) {
            qty = remainingQuantity_;
        }
        remainingQuantity_ -= qty;
        
        if (remainingQuantity_ == 0) {
            status_ = OrderStatus::FILLED;
        } else {
            status_ = OrderStatus::PARTIALLY_FILLED;
        }
    }

    void cancel() {
        status_ = OrderStatus::CANCELLED;
        remainingQuantity_ = 0;
    }

    // Check if order is active
    bool isActive() const {
        return status_ == OrderStatus::NEW || 
               status_ == OrderStatus::PARTIALLY_FILLED;
    }

    // Check if order can match with another order
    bool canMatch(const Order& other) const {
        // Must be opposite sides
        if (side_ == other.side_) return false;
        
        // Must be same symbol
        if (symbol_ != other.symbol_) return false;
        
        // Must have remaining quantity
        if (remainingQuantity_ == 0 || other.remainingQuantity_ == 0) {
            return false;
        }

        // Price matching logic
        if (type_ == OrderType::MARKET || other.type_ == OrderType::MARKET) {
            return true;  // Market orders always match
        }

        // For limit orders, check price compatibility
        if (side_ == Side::BUY) {
            // Buy order price must be >= sell order price
            return price_ >= other.price_;
        } else {
            // Sell order price must be <= buy order price
            return price_ <= other.price_;
        }
    }

    // String representation for logging
    std::string toString() const {
        return "Order[id=" + std::to_string(id_) +
               ", symbol=" + symbol_ +
               ", side=" + sideToString(side_) +
               ", type=" + orderTypeToString(type_) +
               ", price=" + std::to_string(priceToDouble(price_)) +
               ", qty=" + std::to_string(quantity_) +
               ", remaining=" + std::to_string(remainingQuantity_) +
               ", status=" + orderStatusToString(status_) + "]";
    }

private:
    OrderId id_;
    Symbol symbol_;
    Side side_;
    OrderType type_;
    Price price_;
    Quantity quantity_;
    Quantity remainingQuantity_;
    OrderStatus status_;
    Timestamp timestamp_;

    // Get current timestamp in nanoseconds
    static Timestamp getCurrentTimestamp() {
        auto now = std::chrono::high_resolution_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()
        );
        return static_cast<Timestamp>(nanos.count());
    }
};

} // namespace trading

#endif // ORDER_HPP