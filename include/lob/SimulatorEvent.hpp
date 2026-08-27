#ifndef LOB_SIMULATOR_EVENT_HPP
#define LOB_SIMULATOR_EVENT_HPP

#include "lob/Types.hpp"

namespace lob {

enum class ActionType : std::uint8_t {
    SubmitLimit,
    SubmitMarket,
    Cancel
};

inline std::string to_string(ActionType action) {
    switch (action) {
        case ActionType::SubmitLimit: return "SUBMIT_LIMIT";
        case ActionType::SubmitMarket: return "SUBMIT_MARKET";
        case ActionType::Cancel: return "CANCEL";
    }
    return "UNKNOWN";
}

inline std::ostream& operator<<(std::ostream& os, ActionType action) {
    return os << to_string(action);
}

struct SimulatorEvent {
    ActionType action;
    OrderId order_id;
    Side side;
    OrderType order_type;
    Price price;
    Quantity quantity;
    Timestamp timestamp;
};

} // namespace lob

#endif // LOB_SIMULATOR_EVENT_HPP
