#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include "core/order.hpp"
#include "engine/price_level.hpp"
#include <map>
#include <unordered_map>
#include <memory>
#include <optional>
#include <sstream>

namespace trading {

/**
 * OrderBook maintains bid and ask sides of the market.
 * Bids are sorted descending (highest price first)
 * Asks are sorted ascending (lowest price first)
 */
class OrderBook {
public:
    explicit OrderBook(const Symbol& symbol)
        : symbol_(symbol)
    {}

    // Add an order to the book
    bool addOrder(std::shared_ptr<Order> order) {
        if (order->getSymbol() != symbol_) {
            return false;
        }

        OrderId orderId = order->getId();
        
        // Check for duplicate order ID
        if (orderMap_.find(orderId) != orderMap_.end()) {
            return false;
        }

        // Add to appropriate side
        if (order->getSide() == Side::BUY) {
            addToBidSide(order);
        } else {
            addToAskSide(order);
        }

        // Store in order map for fast lookup
        orderMap_[orderId] = order;
        return true;
    }

    // Cancel an order
    bool cancelOrder(OrderId orderId) {
        auto it = orderMap_.find(orderId);
        if (it == orderMap_.end()) {
            return false;
        }

        auto order = it->second;
        order->cancel();

        // Remove from price level
        Price price = order->getPrice();
        if (order->getSide() == Side::BUY) {
            removeFromBidSide(orderId, price);
        } else {
            removeFromAskSide(orderId, price);
        }

        // Remove from order map
        orderMap_.erase(it);
        return true;
    }

    // Modify an order (cancel and replace)
    bool modifyOrder(OrderId orderId, Price newPrice, Quantity newQuantity) {
        auto it = orderMap_.find(orderId);
        if (it == orderMap_.end()) {
            return false;
        }

        auto oldOrder = it->second;
        
        // Create new order with same ID but new price/quantity
        auto newOrder = std::make_shared<Order>(
            orderId,
            oldOrder->getSymbol(),
            oldOrder->getSide(),
            oldOrder->getType(),
            newPrice,
            newQuantity
        );

        // Cancel old order
        cancelOrder(orderId);
        
        // Add new order
        return addOrder(newOrder);
    }

    // Get best bid price
    std::optional<Price> getBestBid() const {
        if (bids_.empty()) {
            return std::nullopt;
        }
        return bids_.begin()->first;
    }

    // Get best ask price
    std::optional<Price> getBestAsk() const {
        if (asks_.empty()) {
            return std::nullopt;
        }
        return asks_.begin()->first;
    }

    // Get spread (difference between best ask and best bid)
    std::optional<Price> getSpread() const {
        auto bid = getBestBid();
        auto ask = getBestAsk();
        
        if (bid && ask) {
            return *ask - *bid;
        }
        return std::nullopt;
    }

    // Get mid price (average of best bid and ask)
    std::optional<double> getMidPrice() const {
        auto bid = getBestBid();
        auto ask = getBestAsk();
        
        if (bid && ask) {
            return priceToDouble(*bid + *ask) / 2.0;
        }
        return std::nullopt;
    }

    // Get order by ID
    std::shared_ptr<Order> getOrder(OrderId orderId) const {
        auto it = orderMap_.find(orderId);
        return (it != orderMap_.end()) ? it->second : nullptr;
    }

    // Get total bid quantity
    Quantity getTotalBidQuantity() const {
        Quantity total = 0;
        for (const auto& [price, level] : bids_) {
            total += level.getTotalQuantity();
        }
        return total;
    }

    // Get total ask quantity
    Quantity getTotalAskQuantity() const {
        Quantity total = 0;
        for (const auto& [price, level] : asks_) {
            total += level.getTotalQuantity();
        }
        return total;
    }

    // Get market depth (top N levels on each side)
    struct DepthLevel {
        Price price;
        Quantity quantity;
        size_t orderCount;
    };

    std::vector<DepthLevel> getBidDepth(size_t levels = 5) const {
        std::vector<DepthLevel> depth;
        size_t count = 0;
        
        for (const auto& [price, level] : bids_) {
            if (count >= levels) break;
            depth.push_back({price, level.getTotalQuantity(), level.getOrderCount()});
            count++;
        }
        return depth;
    }

    std::vector<DepthLevel> getAskDepth(size_t levels = 5) const {
        std::vector<DepthLevel> depth;
        size_t count = 0;
        
        for (const auto& [price, level] : asks_) {
            if (count >= levels) break;
            depth.push_back({price, level.getTotalQuantity(), level.getOrderCount()});
            count++;
        }
        return depth;
    }

    // Display order book in a readable format
    std::string displayBook(size_t depth = 10) const {
        std::ostringstream oss;
        
        oss << "\n========== ORDER BOOK: " << symbol_ << " ==========\n";
        
        auto askDepth = getAskDepth(depth);
        auto bidDepth = getBidDepth(depth);
        
        // Display asks (top to bottom = lowest to highest)
        oss << "\n--- ASKS ---\n";
        for (auto it = askDepth.rbegin(); it != askDepth.rend(); ++it) {
            oss << "  " << priceToDouble(it->price) 
                << " | " << it->quantity 
                << " (" << it->orderCount << " orders)\n";
        }
        
        // Display spread
        auto spread = getSpread();
        auto mid = getMidPrice();
        if (spread && mid) {
            oss << "\n--- SPREAD: " << priceToDouble(*spread) 
                << " | MID: " << *mid << " ---\n";
        }
        
        // Display bids (top to bottom = highest to lowest)
        oss << "\n--- BIDS ---\n";
        for (const auto& level : bidDepth) {
            oss << "  " << priceToDouble(level.price) 
                << " | " << level.quantity 
                << " (" << level.orderCount << " orders)\n";
        }
        
        oss << "\n==========================================\n";
        return oss.str();
    }

    // Get statistics
    struct BookStats {
        size_t totalOrders;
        size_t bidLevels;
        size_t askLevels;
        Quantity totalBidQty;
        Quantity totalAskQty;
    };

    BookStats getStats() const {
        return {
            orderMap_.size(),
            bids_.size(),
            asks_.size(),
            getTotalBidQuantity(),
            getTotalAskQuantity()
        };
    }

private:
    Symbol symbol_;
    
    // Bids: descending order (highest price first)
    std::map<Price, PriceLevel, std::greater<Price>> bids_;
    
    // Asks: ascending order (lowest price first)
    std::map<Price, PriceLevel, std::less<Price>> asks_;
    
    // Fast order lookup
    std::unordered_map<OrderId, std::shared_ptr<Order>> orderMap_;

    void addToBidSide(std::shared_ptr<Order> order) {
        Price price = order->getPrice();
        
        auto it = bids_.find(price);
        if (it == bids_.end()) {
            bids_.emplace(price, PriceLevel(price));
            it = bids_.find(price);
        }
        
        it->second.addOrder(order);
    }

    void addToAskSide(std::shared_ptr<Order> order) {
        Price price = order->getPrice();
        
        auto it = asks_.find(price);
        if (it == asks_.end()) {
            asks_.emplace(price, PriceLevel(price));
            it = asks_.find(price);
        }
        
        it->second.addOrder(order);
    }

    void removeFromBidSide(OrderId orderId, Price price) {
        auto it = bids_.find(price);
        if (it != bids_.end()) {
            it->second.removeOrder(orderId);
            if (it->second.isEmpty()) {
                bids_.erase(it);
            }
        }
    }

    void removeFromAskSide(OrderId orderId, Price price) {
        auto it = asks_.find(price);
        if (it != asks_.end()) {
            it->second.removeOrder(orderId);
            if (it->second.isEmpty()) {
                asks_.erase(it);
            }
        }
    }
};

} // namespace trading

#endif // ORDER_BOOK_HPP