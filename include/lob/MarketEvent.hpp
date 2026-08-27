#ifndef LOB_MARKET_EVENT_HPP
#define LOB_MARKET_EVENT_HPP

#include "lob/Types.hpp"
#include "lob/Trade.hpp"

namespace lob {

enum class EventType : std::uint8_t {
    OrderAccepted,
    OrderRejected,
    OrderExecuted,
    OrderCancelled,
    TradeGenerated
};

inline std::string to_string(EventType type) {
    switch (type) {
        case EventType::OrderAccepted: return "OrderAccepted";
        case EventType::OrderRejected: return "OrderRejected";
        case EventType::OrderExecuted: return "OrderExecuted";
        case EventType::OrderCancelled: return "OrderCancelled";
        case EventType::TradeGenerated: return "TradeGenerated";
    }
    return "UNKNOWN";
}

inline std::ostream& operator<<(std::ostream& os, EventType type) {
    return os << to_string(type);
}

struct MarketEvent {
    EventType event_type;
    SequenceNumber sequence_number;
    Timestamp timestamp;
    OrderId order_id;
    Side side;
    Price price;
    Quantity quantity;
    Quantity remaining_quantity;
    OrderStatus status;
    Trade trade; // Populated if event_type == EventType::TradeGenerated or OrderExecuted
    std::string reason; // Populated if rejected
};

} // namespace lob

#endif // LOB_MARKET_EVENT_HPP
