#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>
#include <string>

namespace trading {

// Type aliases for clarity and maintainability
using OrderId = uint64_t;
using Price = int64_t;        // Fixed-point: divide by 100 for actual price
using Quantity = uint64_t;
using Timestamp = uint64_t;   // Nanoseconds since epoch
using Symbol = std::string;

// Order side enumeration
enum class Side : uint8_t {
    BUY = 0,
    SELL = 1
};

// Order type enumeration
enum class OrderType : uint8_t {
    MARKET = 0,
    LIMIT = 1,
    STOP = 2,
    STOP_LIMIT = 3
};

// Order status enumeration
enum class OrderStatus : uint8_t {
    NEW = 0,
    PARTIALLY_FILLED = 1,
    FILLED = 2,
    CANCELLED = 3,
    REJECTED = 4
};

// Trade side (aggressor)
enum class TradeSide : uint8_t {
    BUY = 0,
    SELL = 1
};

// Convert enums to strings for logging
inline const char* sideToString(Side side) {
    return side == Side::BUY ? "BUY" : "SELL";
}

inline const char* orderTypeToString(OrderType type) {
    switch (type) {
        case OrderType::MARKET: return "MARKET";
        case OrderType::LIMIT: return "LIMIT";
        case OrderType::STOP: return "STOP";
        case OrderType::STOP_LIMIT: return "STOP_LIMIT";
        default: return "UNKNOWN";
    }
}

inline const char* orderStatusToString(OrderStatus status) {
    switch (status) {
        case OrderStatus::NEW: return "NEW";
        case OrderStatus::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
        case OrderStatus::FILLED: return "FILLED";
        case OrderStatus::CANCELLED: return "CANCELLED";
        case OrderStatus::REJECTED: return "REJECTED";
        default: return "UNKNOWN";
    }
}

// Price conversion utilities
inline double priceToDouble(Price price) {
    return static_cast<double>(price) / 100.0;
}

inline Price doubleToPrice(double price) {
    return static_cast<Price>(price * 100.0);
}

} // namespace trading

#endif // TYPES_HPP