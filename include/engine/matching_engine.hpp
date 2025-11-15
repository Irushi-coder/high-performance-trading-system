#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP

#include "core/order.hpp"
#include "core/trade.hpp"
#include "engine/order_book.hpp"
#include "utils/logger.hpp"
#include <vector>
#include <memory>
#include <functional>

namespace trading {

/**
 * MatchingEngine executes trades by matching incoming orders
 * against the order book using price-time priority.
 */
class MatchingEngine {
public:
    // Callback for trade notifications
    using TradeCallback = std::function<void(const Trade&)>;
    
    // Callback for order updates (fills, cancellations)
    using OrderUpdateCallback = std::function<void(std::shared_ptr<Order>)>;

    explicit MatchingEngine(const Symbol& symbol)
        : orderBook_(symbol)
        , symbol_(symbol)
        , nextOrderId_(1)
    {}

    // Submit a new order and return trades generated
    std::vector<Trade> submitOrder(std::shared_ptr<Order> order) {
        std::vector<Trade> trades;

        if (order->getSymbol() != symbol_) {
            LOG_ERROR("Order symbol mismatch: ", order->getSymbol(), 
                           " vs ", symbol_);
            return trades;
        }

        // Match based on order type
        if (order->getType() == OrderType::MARKET) {
            trades = matchMarketOrder(order);
        } else if (order->getType() == OrderType::LIMIT) {
            trades = matchLimitOrder(order);
        }

        // Notify callbacks
        for (const auto& trade : trades) {
            if (tradeCallback_) {
                tradeCallback_(trade);
            }
        }

        return trades;
    }

    // Cancel an order
    bool cancelOrder(OrderId orderId) {
        return orderBook_.cancelOrder(orderId);
    }

    // Modify an order (cancel and replace)
    bool modifyOrder(OrderId orderId, Price newPrice, Quantity newQuantity) {
        return orderBook_.modifyOrder(orderId, newPrice, newQuantity);
    }

    // Get the order book
    const OrderBook& getOrderBook() const { return orderBook_; }
    OrderBook& getOrderBook() { return orderBook_; }

    // Set callbacks
    void setTradeCallback(TradeCallback callback) {
        tradeCallback_ = std::move(callback);
    }

    void setOrderUpdateCallback(OrderUpdateCallback callback) {
        orderUpdateCallback_ = std::move(callback);
    }

    // Generate next order ID
    OrderId getNextOrderId() { return nextOrderId_++; }

    // Get matching statistics
    struct MatchingStats {
        uint64_t totalTrades;
        uint64_t totalVolume;
        double totalValue;
        uint64_t marketOrdersMatched;
        uint64_t limitOrdersMatched;
    };

    MatchingStats getStats() const { return stats_; }

private:
    OrderBook orderBook_;
    Symbol symbol_;
    OrderId nextOrderId_;
    MatchingStats stats_{};
    TradeCallback tradeCallback_;
    OrderUpdateCallback orderUpdateCallback_;

    /**
     * Match a market order against the book.
     * Market orders execute immediately at the best available prices.
     */
    std::vector<Trade> matchMarketOrder(std::shared_ptr<Order> order) {
        std::vector<Trade> trades;

        if (order->getSide() == Side::BUY) {
            trades = matchMarketBuyOrder(order);
        } else {
            trades = matchMarketSellOrder(order);
        }

        stats_.marketOrdersMatched++;
        return trades;
    }

    /**
     * Match a market buy order (takes from ask side).
     */
    std::vector<Trade> matchMarketBuyOrder(std::shared_ptr<Order> order) {
        std::vector<Trade> trades;
        Quantity remaining = order->getRemainingQuantity();

        // Get ask depth (sellers)
        auto askDepth = orderBook_.getAskDepth(100); // Get up to 100 levels

        for (const auto& level : askDepth) {
            if (remaining == 0) break;

            Price levelPrice = level.price;
            
            // Match against orders at this price level
            while (remaining > 0) {
                auto bestAsk = orderBook_.getBestAsk();
                if (!bestAsk || *bestAsk != levelPrice) break;

                // Get the best ask order
                auto sellOrder = getBestAskOrder();
                if (!sellOrder) break;

                // Calculate fill quantity
                Quantity fillQty = std::min(remaining, 
                                           sellOrder->getRemainingQuantity());

                // Create trade
                Trade trade(order->getId(), sellOrder->getId(), 
                          symbol_, levelPrice, fillQty);
                trades.push_back(trade);

                // Update orders
                order->fillQuantity(fillQty);
                sellOrder->fillQuantity(fillQty);
                remaining -= fillQty;

                // Update order book
                if (sellOrder->getRemainingQuantity() == 0) {
                    orderBook_.cancelOrder(sellOrder->getId());
                }

                // Update stats
                stats_.totalTrades++;
                stats_.totalVolume += fillQty;
                stats_.totalValue += trade.getValue();

                // Notify callbacks
                if (orderUpdateCallback_) {
                    orderUpdateCallback_(sellOrder);
                }
            }
        }

        // If order still has remaining quantity, it couldn't be fully filled
        if (remaining > 0) {
            LOG_WARN("Market buy order ", order->getId(), 
                          " only partially filled. Remaining: ", remaining);
        }

        if (orderUpdateCallback_) {
            orderUpdateCallback_(order);
        }

        return trades;
    }

    /**
     * Match a market sell order (takes from bid side).
     */
    std::vector<Trade> matchMarketSellOrder(std::shared_ptr<Order> order) {
        std::vector<Trade> trades;
        Quantity remaining = order->getRemainingQuantity();

        // Get bid depth (buyers)
        auto bidDepth = orderBook_.getBidDepth(100);

        for (const auto& level : bidDepth) {
            if (remaining == 0) break;

            Price levelPrice = level.price;

            while (remaining > 0) {
                auto bestBid = orderBook_.getBestBid();
                if (!bestBid || *bestBid != levelPrice) break;

                auto buyOrder = getBestBidOrder();
                if (!buyOrder) break;

                Quantity fillQty = std::min(remaining, 
                                           buyOrder->getRemainingQuantity());

                Trade trade(buyOrder->getId(), order->getId(), 
                          symbol_, levelPrice, fillQty);
                trades.push_back(trade);

                order->fillQuantity(fillQty);
                buyOrder->fillQuantity(fillQty);
                remaining -= fillQty;

                if (buyOrder->getRemainingQuantity() == 0) {
                    orderBook_.cancelOrder(buyOrder->getId());
                }

                stats_.totalTrades++;
                stats_.totalVolume += fillQty;
                stats_.totalValue += trade.getValue();

                if (orderUpdateCallback_) {
                    orderUpdateCallback_(buyOrder);
                }
            }
        }

        if (remaining > 0) {
            LOG_WARN("Market sell order ", order->getId(), 
                          " only partially filled. Remaining: ", remaining);
        }

        if (orderUpdateCallback_) {
            orderUpdateCallback_(order);
        }

        return trades;
    }

    /**
     * Match a limit order against the book.
     * Limit orders only execute at their limit price or better.
     */
    std::vector<Trade> matchLimitOrder(std::shared_ptr<Order> order) {
        std::vector<Trade> trades;

        if (order->getSide() == Side::BUY) {
            trades = matchLimitBuyOrder(order);
        } else {
            trades = matchLimitSellOrder(order);
        }

        // Add remaining quantity to book if not fully filled
        if (order->getRemainingQuantity() > 0) {
            orderBook_.addOrder(order);
        }

        stats_.limitOrdersMatched++;
        return trades;
    }

    /**
     * Match a limit buy order.
     */
    std::vector<Trade> matchLimitBuyOrder(std::shared_ptr<Order> order) {
        std::vector<Trade> trades;
        Quantity remaining = order->getRemainingQuantity();
        Price limitPrice = order->getPrice();

        while (remaining > 0) {
            auto bestAsk = orderBook_.getBestAsk();
            
            // No more asks or price doesn't match
            if (!bestAsk || *bestAsk > limitPrice) break;

            auto sellOrder = getBestAskOrder();
            if (!sellOrder) break;

            Quantity fillQty = std::min(remaining, 
                                       sellOrder->getRemainingQuantity());
            Price tradePrice = *bestAsk; // Trade at ask price (better for buyer)

            Trade trade(order->getId(), sellOrder->getId(), 
                      symbol_, tradePrice, fillQty);
            trades.push_back(trade);

            order->fillQuantity(fillQty);
            sellOrder->fillQuantity(fillQty);
            remaining -= fillQty;

            if (sellOrder->getRemainingQuantity() == 0) {
                orderBook_.cancelOrder(sellOrder->getId());
            }

            stats_.totalTrades++;
            stats_.totalVolume += fillQty;
            stats_.totalValue += trade.getValue();

            if (orderUpdateCallback_) {
                orderUpdateCallback_(sellOrder);
            }
        }

        if (orderUpdateCallback_) {
            orderUpdateCallback_(order);
        }

        return trades;
    }

    /**
     * Match a limit sell order.
     */
    std::vector<Trade> matchLimitSellOrder(std::shared_ptr<Order> order) {
        std::vector<Trade> trades;
        Quantity remaining = order->getRemainingQuantity();
        Price limitPrice = order->getPrice();

        while (remaining > 0) {
            auto bestBid = orderBook_.getBestBid();
            
            // No more bids or price doesn't match
            if (!bestBid || *bestBid < limitPrice) break;

            auto buyOrder = getBestBidOrder();
            if (!buyOrder) break;

            Quantity fillQty = std::min(remaining, 
                                       buyOrder->getRemainingQuantity());
            Price tradePrice = *bestBid; // Trade at bid price (better for seller)

            Trade trade(buyOrder->getId(), order->getId(), 
                      symbol_, tradePrice, fillQty);
            trades.push_back(trade);

            order->fillQuantity(fillQty);
            buyOrder->fillQuantity(fillQty);
            remaining -= fillQty;

            if (buyOrder->getRemainingQuantity() == 0) {
                orderBook_.cancelOrder(buyOrder->getId());
            }

            stats_.totalTrades++;
            stats_.totalVolume += fillQty;
            stats_.totalValue += trade.getValue();

            if (orderUpdateCallback_) {
                orderUpdateCallback_(buyOrder);
            }
        }

        if (orderUpdateCallback_) {
            orderUpdateCallback_(order);
        }

        return trades;
    }

    // Helper to get best bid order (private access to order book internals)
    std::shared_ptr<Order> getBestBidOrder() {
        auto bidDepth = orderBook_.getBidDepth(1);
        if (bidDepth.empty()) return nullptr;
        
        // This is a simplified version - in reality, we'd need direct access
        // For now, we'll work with the order book's public interface
        // In a production system, we'd make MatchingEngine a friend class
        return nullptr; // Placeholder - will be implemented with better access
    }

    std::shared_ptr<Order> getBestAskOrder() {
        auto askDepth = orderBook_.getAskDepth(1);
        if (askDepth.empty()) return nullptr;
        return nullptr; // Placeholder
    }
};

} // namespace trading

#endif // MATCHING_ENGINE_HPP