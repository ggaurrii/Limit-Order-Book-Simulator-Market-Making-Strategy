#ifndef LOB_TYPES_HPP
#define LOB_TYPES_HPP

#include <cstdint>
#include <string>
#include <iostream>

namespace lob {

using Price = std::int64_t;     // Prices represented as ticks / fixed-point units
using Quantity = std::uint64_t; // Order volume / lot size
using OrderId = std::uint64_t;  // Unique order identifier
using SequenceNumber = std::uint64_t; // Event sequence ID
using Timestamp = std::uint64_t;      // Nanoseconds or tick timestamp

enum class Side : std::uint8_t {
    Buy,
    Sell,
    Unknown
};

enum class OrderType : std::uint8_t {
    Limit,
    Market
};

enum class TimeInForce : std::uint8_t {
    GTC, // Good 'Til Cancelled
    IOC  // Immediate Or Cancel
};

enum class OrderStatus : std::uint8_t {
    New,
    PartiallyFilled,
    Filled,
    Cancelled,
    Rejected
};

inline std::string to_string(Side side) {
    switch (side) {
        case Side::Buy: return "BUY";
        case Side::Sell: return "SELL";
        case Side::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

inline std::string to_string(OrderType type) {
    switch (type) {
        case OrderType::Limit: return "LIMIT";
        case OrderType::Market: return "MARKET";
    }
    return "UNKNOWN";
}

inline std::string to_string(OrderStatus status) {
    switch (status) {
        case OrderStatus::New: return "NEW";
        case OrderStatus::PartiallyFilled: return "PARTIALLY_FILLED";
        case OrderStatus::Filled: return "FILLED";
        case OrderStatus::Cancelled: return "CANCELLED";
        case OrderStatus::Rejected: return "REJECTED";
    }
    return "UNKNOWN";
}

inline std::ostream& operator<<(std::ostream& os, Side side) {
    return os << to_string(side);
}

inline std::ostream& operator<<(std::ostream& os, OrderType type) {
    return os << to_string(type);
}

inline std::ostream& operator<<(std::ostream& os, TimeInForce tif) {
    switch (tif) {
        case TimeInForce::GTC: return os << "GTC";
        case TimeInForce::IOC: return os << "IOC";
    }
    return os << "UNKNOWN";
}

inline std::ostream& operator<<(std::ostream& os, OrderStatus status) {
    return os << to_string(status);
}

} // namespace lob

#endif // LOB_TYPES_HPP
