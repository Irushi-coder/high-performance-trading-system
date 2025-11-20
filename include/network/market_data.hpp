#ifndef MARKET_DATA_HPP
#define MARKET_DATA_HPP

#include "core/types.hpp"
#include "core/trade.hpp"
#include "engine/order_book.hpp"
#include "network/tcp_server.hpp"
#include <string>
#include <sstream>
#include <iomanip>
#include <memory>

namespace trading {
namespace network {

/**
 * MarketDataPublisher streams real-time order book and trade data.
 */
class MarketDataPublisher {
public:
    MarketDataPublisher() = default;

    /**
     * Publish order book snapshot in JSON format.
     */
    static std::string formatOrderBookSnapshot(const OrderBook& book) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        
        oss << "{\n";
        oss << "  \"type\": \"orderbook_snapshot\",\n";
        oss << "  \"timestamp\": " << getCurrentTimestamp() << ",\n";
        
        // Best bid/ask
        auto bestBid = book.getBestBid();
        auto bestAsk = book.getBestAsk();
        
        if (bestBid) {
            oss << "  \"best_bid\": " << priceToDouble(*bestBid) << ",\n";
        }
        if (bestAsk) {
            oss << "  \"best_ask\": " << priceToDouble(*bestAsk) << ",\n";
        }
        
        auto spread = book.getSpread();
        auto mid = book.getMidPrice();
        
        if (spread) {
            oss << "  \"spread\": " << priceToDouble(*spread) << ",\n";
        }
        if (mid) {
            oss << "  \"mid_price\": " << *mid << ",\n";
        }
        
        // Bids
        auto bids = book.getBidDepth(10);
        oss << "  \"bids\": [\n";
        for (size_t i = 0; i < bids.size(); ++i) {
            oss << "    {\"price\": " << priceToDouble(bids[i].price)
                << ", \"quantity\": " << bids[i].quantity
                << ", \"orders\": " << bids[i].orderCount << "}";
            if (i < bids.size() - 1) oss << ",";
            oss << "\n";
        }
        oss << "  ],\n";
        
        // Asks
        auto asks = book.getAskDepth(10);
        oss << "  \"asks\": [\n";
        for (size_t i = 0; i < asks.size(); ++i) {
            oss << "    {\"price\": " << priceToDouble(asks[i].price)
                << ", \"quantity\": " << asks[i].quantity
                << ", \"orders\": " << asks[i].orderCount << "}";
            if (i < asks.size() - 1) oss << ",";
            oss << "\n";
        }
        oss << "  ]\n";
        
        oss << "}\n";
        return oss.str();
    }

    /**
     * Publish trade in JSON format.
     */
    static std::string formatTrade(const Trade& trade) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        
        oss << "{\n";
        oss << "  \"type\": \"trade\",\n";
        oss << "  \"timestamp\": " << trade.getTimestamp() << ",\n";
        oss << "  \"symbol\": \"" << trade.getSymbol() << "\",\n";
        oss << "  \"buy_order_id\": " << trade.getBuyOrderId() << ",\n";
        oss << "  \"sell_order_id\": " << trade.getSellOrderId() << ",\n";
        oss << "  \"price\": " << priceToDouble(trade.getPrice()) << ",\n";
        oss << "  \"quantity\": " << trade.getQuantity() << ",\n";
        oss << "  \"value\": " << trade.getValue() << "\n";
        oss << "}\n";
        
        return oss.str();
    }

    /**
     * Publish statistics in JSON format.
     */
    static std::string formatStats(const OrderBook& book) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        
        auto stats = book.getStats();
        
        oss << "{\n";
        oss << "  \"type\": \"statistics\",\n";
        oss << "  \"timestamp\": " << getCurrentTimestamp() << ",\n";
        oss << "  \"total_orders\": " << stats.totalOrders << ",\n";
        oss << "  \"bid_levels\": " << stats.bidLevels << ",\n";
        oss << "  \"ask_levels\": " << stats.askLevels << ",\n";
        oss << "  \"total_bid_quantity\": " << stats.totalBidQty << ",\n";
        oss << "  \"total_ask_quantity\": " << stats.totalAskQty << "\n";
        oss << "}\n";
        
        return oss.str();
    }

    /**
     * Format as CSV for logging.
     */
    static std::string formatTradeCSV(const Trade& trade) {
        std::ostringstream oss;
        oss << trade.getTimestamp() << ","
            << trade.getSymbol() << ","
            << trade.getBuyOrderId() << ","
            << trade.getSellOrderId() << ","
            << std::fixed << std::setprecision(2)
            << priceToDouble(trade.getPrice()) << ","
            << trade.getQuantity() << ","
            << trade.getValue() << "\n";
        return oss.str();
    }

    /**
     * Create simple order book display for text clients.
     */
    static std::string formatOrderBookText(const OrderBook& book) {
        std::ostringstream oss;
        
        oss << "\n===== ORDER BOOK =====\n";
        
        auto asks = book.getAskDepth(5);
        oss << "\nASKS:\n";
        for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
            oss << std::fixed << std::setprecision(2)
                << "  $" << priceToDouble(it->price)
                << " | " << it->quantity
                << " (" << it->orderCount << " orders)\n";
        }
        
        auto bestBid = book.getBestBid();
        auto bestAsk = book.getBestAsk();
        
        if (bestBid && bestAsk) {
            oss << "\nSPREAD: $" << std::fixed << std::setprecision(2)
                << priceToDouble(*bestAsk - *bestBid) << "\n";
        }
        
        oss << "\nBIDS:\n";
        auto bids = book.getBidDepth(5);
        for (const auto& level : bids) {
            oss << std::fixed << std::setprecision(2)
                << "  $" << priceToDouble(level.price)
                << " | " << level.quantity
                << " (" << level.orderCount << " orders)\n";
        }
        
        oss << "=====================\n";
        return oss.str();
    }

private:
    static uint64_t getCurrentTimestamp() {
        auto now = std::chrono::high_resolution_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()
        );
        return static_cast<uint64_t>(nanos.count());
    }
};

} // namespace network
} // namespace trading

#endif // MARKET_DATA_HPP