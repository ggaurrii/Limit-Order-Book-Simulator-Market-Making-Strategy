#ifndef LOB_ORDER_HPP
#define LOB_ORDER_HPP

#include "lob/Types.hpp"

namespace lob {

struct Order {
    OrderId id;
    Side side;
    OrderType type;
    Price price;
    Quantity initial_quantity;
    Quantity remaining_quantity;
    Timestamp timestamp;

    Order(OrderId id_, Side side_, OrderType type_, Price price_, Quantity quantity_, Timestamp timestamp_)
        : id(id_)
        , side(side_)
        , type(type_)
        , price(price_)
        , initial_quantity(quantity_)
        , remaining_quantity(quantity_)
        , timestamp(timestamp_) {}

    bool is_filled() const {
        return remaining_quantity == 0;
    }
};

} // namespace lob

#endif // LOB_ORDER_HPP
