#ifndef FIX_MESSAGE_HPP
#define FIX_MESSAGE_HPP

#include "core/types.hpp"
#include "core/order.hpp"
#include <string>
#include <unordered_map>
#include <sstream>
#include <memory>

namespace trading {
namespace network {

/**
 * FIX Protocol Message Parser and Builder.
 * Implements a subset of FIX 4.2 for order entry.
 */
class FIXMessage {
public:
    // Common FIX tags
    static constexpr int TAG_BEGIN_STRING = 8;    // FIX version
    static constexpr int TAG_BODY_LENGTH = 9;     // Message length
    static constexpr int TAG_MSG_TYPE = 35;       // Message type
    static constexpr int TAG_SENDER_COMP_ID = 49; // Sender ID
    static constexpr int TAG_TARGET_COMP_ID = 56; // Target ID
    static constexpr int TAG_MSG_SEQ_NUM = 34;    // Sequence number
    static constexpr int TAG_SENDING_TIME = 52;   // Timestamp
    static constexpr int TAG_CHECKSUM = 10;       // Checksum
    
    // Order-related tags
    static constexpr int TAG_CLORD_ID = 11;       // Client order ID
    static constexpr int TAG_SYMBOL = 55;         // Ticker symbol
    static constexpr int TAG_SIDE = 54;           // Buy/Sell
    static constexpr int TAG_ORDER_QTY = 38;      // Quantity
    static constexpr int TAG_ORD_TYPE = 40;       // Order type
    static constexpr int TAG_PRICE = 44;          // Price
    static constexpr int TAG_EXEC_TYPE = 150;     // Execution type
    static constexpr int TAG_ORDER_ID = 37;       // Order ID
    static constexpr int TAG_EXEC_ID = 17;        // Execution ID
    static constexpr int TAG_LAST_PX = 31;        // Last price
    static constexpr int TAG_LAST_QTY = 32;       // Last quantity
    static constexpr int TAG_CUM_QTY = 14;        // Cumulative quantity
    static constexpr int TAG_LEAVES_QTY = 151;    // Remaining quantity
    
    // Message types
    static constexpr char MSG_NEW_ORDER = 'D';
    static constexpr char MSG_CANCEL = 'F';
    static constexpr char MSG_MODIFY = 'G';
    static constexpr char MSG_EXEC_REPORT = '8';
    static constexpr char MSG_REJECT = '3';
    static constexpr char MSG_HEARTBEAT = '0';
    static constexpr char MSG_LOGON = 'A';
    static constexpr char MSG_LOGOUT = '5';

    FIXMessage() = default;

    /**
     * Parse FIX message from string.
     * FIX format: 8=FIX.4.2|9=123|35=D|...|10=123|
     * (using | for SOH delimiter in display)
     */
    static FIXMessage parse(const std::string& rawMessage) {
        FIXMessage msg;
        std::istringstream iss(rawMessage);
        std::string token;
        
        while (std::getline(iss, token, '\x01')) { // SOH delimiter
            size_t pos = token.find('=');
            if (pos != std::string::npos) {
                int tag = std::stoi(token.substr(0, pos));
                std::string value = token.substr(pos + 1);
                msg.setField(tag, value);
            }
        }
        
        return msg;
    }

    /**
     * Serialize to FIX string.
     */
    std::string serialize() const {
        std::ostringstream oss;
        
        // Header (in correct order)
        oss << TAG_BEGIN_STRING << "=FIX.4.2\x01";
        
        // Calculate body length (placeholder)
        std::ostringstream body;
        for (const auto& [tag, value] : fields_) {
            if (tag != TAG_BEGIN_STRING && tag != TAG_BODY_LENGTH && 
                tag != TAG_CHECKSUM) {
                body << tag << "=" << value << "\x01";
            }
        }
        
        std::string bodyStr = body.str();
        oss << TAG_BODY_LENGTH << "=" << bodyStr.length() << "\x01";
        oss << bodyStr;
        
        // Calculate checksum
        std::string msgWithoutChecksum = oss.str();
        int checksum = calculateChecksum(msgWithoutChecksum);
        oss << TAG_CHECKSUM << "=" << std::to_string(checksum) << "\x01";
        
        return oss.str();
    }

    // Field accessors
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return (it != fields_.end()) ? it->second : "";
    }

    int getFieldAsInt(int tag) const {
        std::string value = getField(tag);
        return value.empty() ? 0 : std::stoi(value);
    }

    double getFieldAsDouble(int tag) const {
        std::string value = getField(tag);
        return value.empty() ? 0.0 : std::stod(value);
    }

    bool hasField(int tag) const {
        return fields_.find(tag) != fields_.end();
    }

    // Message type helpers
    char getMessageType() const {
        std::string type = getField(TAG_MSG_TYPE);
        return type.empty() ? '\0' : type[0];
    }

    void setMessageType(char type) {
        setField(TAG_MSG_TYPE, std::string(1, type));
    }

    /**
     * Convert FIX message to Order object.
     */
    std::shared_ptr<Order> toOrder() const {
        if (getMessageType() != MSG_NEW_ORDER) {
            return nullptr;
        }

        OrderId orderId = getFieldAsInt(TAG_CLORD_ID);
        Symbol symbol = getField(TAG_SYMBOL);
        
        // Parse side
        char sideChar = getField(TAG_SIDE)[0];
        Side side = (sideChar == '1') ? Side::BUY : Side::SELL;
        
        // Parse order type
        char typeChar = getField(TAG_ORD_TYPE)[0];
        OrderType orderType = OrderType::LIMIT;
        if (typeChar == '1') orderType = OrderType::MARKET;
        else if (typeChar == '2') orderType = OrderType::LIMIT;
        
        Quantity quantity = getFieldAsInt(TAG_ORDER_QTY);
        
        if (orderType == OrderType::MARKET) {
            return std::make_shared<Order>(orderId, symbol, side, quantity);
        } else {
            double priceDouble = getFieldAsDouble(TAG_PRICE);
            Price price = doubleToPrice(priceDouble);
            return std::make_shared<Order>(
                orderId, symbol, side, orderType, price, quantity
            );
        }
    }

    /**
     * Create FIX execution report from Order and Trade.
     */
    static FIXMessage createExecutionReport(
        const Order& order, 
        const std::string& execId,
        char execType = '0',  // 0=New, 1=Partial, 2=Fill
        Quantity lastQty = 0,
        Price lastPx = 0
    ) {
        FIXMessage msg;
        msg.setMessageType(MSG_EXEC_REPORT);
        
        msg.setField(TAG_ORDER_ID, std::to_string(order.getId()));
        msg.setField(TAG_CLORD_ID, std::to_string(order.getId()));
        msg.setField(TAG_EXEC_ID, execId);
        msg.setField(TAG_EXEC_TYPE, std::string(1, execType));
        msg.setField(TAG_SYMBOL, order.getSymbol());
        
        char sideChar = (order.getSide() == Side::BUY) ? '1' : '2';
        msg.setField(TAG_SIDE, std::string(1, sideChar));
        
        msg.setField(TAG_ORDER_QTY, std::to_string(order.getQuantity()));
        msg.setField(TAG_LEAVES_QTY, std::to_string(order.getRemainingQuantity()));
        
        Quantity cumQty = order.getQuantity() - order.getRemainingQuantity();
        msg.setField(TAG_CUM_QTY, std::to_string(cumQty));
        
        if (lastQty > 0) {
            msg.setField(TAG_LAST_QTY, std::to_string(lastQty));
            msg.setField(TAG_LAST_PX, std::to_string(priceToDouble(lastPx)));
        }
        
        return msg;
    }

    /**
     * Create FIX new order message.
     */
    static FIXMessage createNewOrder(
        OrderId clOrdId,
        const Symbol& symbol,
        Side side,
        OrderType type,
        Quantity quantity,
        Price price = 0
    ) {
        FIXMessage msg;
        msg.setMessageType(MSG_NEW_ORDER);
        
        msg.setField(TAG_CLORD_ID, std::to_string(clOrdId));
        msg.setField(TAG_SYMBOL, symbol);
        
        char sideChar = (side == Side::BUY) ? '1' : '2';
        msg.setField(TAG_SIDE, std::string(1, sideChar));
        
        char typeChar = (type == OrderType::MARKET) ? '1' : '2';
        msg.setField(TAG_ORD_TYPE, std::string(1, typeChar));
        
        msg.setField(TAG_ORDER_QTY, std::to_string(quantity));
        
        if (type == OrderType::LIMIT) {
            msg.setField(TAG_PRICE, std::to_string(priceToDouble(price)));
        }
        
        return msg;
    }

    // String representation for debugging
    std::string toString() const {
        std::ostringstream oss;
        oss << "FIXMessage[type=" << getMessageType() << ", fields={";
        
        bool first = true;
        for (const auto& [tag, value] : fields_) {
            if (!first) oss << ", ";
            oss << tag << "=" << value;
            first = false;
        }
        
        oss << "}]";
        return oss.str();
    }

private:
    std::unordered_map<int, std::string> fields_;

    static int calculateChecksum(const std::string& message) {
        int sum = 0;
        for (char c : message) {
            sum += static_cast<unsigned char>(c);
        }
        return sum % 256;
    }
};

} // namespace network
} // namespace trading

#endif // FIX_MESSAGE_HPP