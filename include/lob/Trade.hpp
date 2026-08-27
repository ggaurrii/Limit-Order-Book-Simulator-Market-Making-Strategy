#ifndef LOB_TRADE_HPP
#define LOB_TRADE_HPP

#include "lob/Types.hpp"

namespace lob {

struct Trade {
    std::uint64_t trade_id;
    OrderId buy_order_id;
    OrderId sell_order_id;
    Price price;
    Quantity quantity;
    Timestamp timestamp;
    Side maker_side; // The side of the passive order resting in the book
};

} // namespace lob

#endif // LOB_TRADE_HPP
