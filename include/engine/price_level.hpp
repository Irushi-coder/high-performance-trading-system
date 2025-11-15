#ifndef PRICE_LEVEL_HPP
#define PRICE_LEVEL_HPP

#include "core/order.hpp"
#include <deque>
#include <memory>
#include <algorithm>
#include <stdexcept>

namespace trading {

/**
 * PriceLevel manages all orders at a specific price point.
 * Orders are maintained in FIFO (First In, First Out) order.
 */
class PriceLevel {
public:
    explicit PriceLevel(Price price) 
        : price_(price)
        , totalQuantity_(0)
    {}

    // Add an order to this price level
    void addOrder(std::shared_ptr<Order> order) {
        if (order->getPrice() != price_) {
            throw std::runtime_error("Order price doesn't match price level");
        }
        
        orders_.push_back(order);
        totalQuantity_ += order->getRemainingQuantity();
    }

    // Remove an order by ID
    bool removeOrder(OrderId orderId) {
        auto it = std::find_if(orders_.begin(), orders_.end(),
            [orderId](const std::shared_ptr<Order>& order) {
                return order->getId() == orderId;
            });

        if (it != orders_.end()) {
            totalQuantity_ -= (*it)->getRemainingQuantity();
            orders_.erase(it);
            return true;
        }
        return false;
    }

    // Update quantity after a fill
    void updateQuantity(OrderId orderId, Quantity filledQty) {
        auto it = std::find_if(orders_.begin(), orders_.end(),
            [orderId](const std::shared_ptr<Order>& order) {
                return order->getId() == orderId;
            });

        if (it != orders_.end()) {
            totalQuantity_ -= filledQty;
            
            // Remove order if fully filled
            if ((*it)->getRemainingQuantity() == 0) {
                orders_.erase(it);
            }
        }
    }

    // Get the first order in the queue (FIFO)
    std::shared_ptr<Order> getFrontOrder() const {
        return orders_.empty() ? nullptr : orders_.front();
    }

    // Get all orders at this level
    const std::deque<std::shared_ptr<Order>>& getOrders() const {
        return orders_;
    }

    // Get total quantity at this price level
    Quantity getTotalQuantity() const {
        return totalQuantity_;
    }

    // Get the price of this level
    Price getPrice() const {
        return price_;
    }

    // Check if this level is empty
    bool isEmpty() const {
        return orders_.empty();
    }

    // Get number of orders at this level
    size_t getOrderCount() const {
        return orders_.size();
    }

    // String representation for debugging
    std::string toString() const {
        return "PriceLevel[price=" + std::to_string(priceToDouble(price_)) +
               ", orders=" + std::to_string(orders_.size()) +
               ", totalQty=" + std::to_string(totalQuantity_) + "]";
    }

private:
    Price price_;
    Quantity totalQuantity_;
    std::deque<std::shared_ptr<Order>> orders_;  // FIFO queue
};

} // namespace trading

#endif // PRICE_LEVEL_HPP