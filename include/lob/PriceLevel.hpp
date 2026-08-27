#ifndef LOB_PRICE_LEVEL_HPP
#define LOB_PRICE_LEVEL_HPP

#include "lob/Order.hpp"
#include <list>
#include <cstddef>

namespace lob {

class PriceLevel {
public:
    using OrderList = std::list<Order>;
    using OrderIterator = OrderList::iterator;

    explicit PriceLevel(Price price_)
        : m_price(price_)
        , m_total_volume(0) {}

    Price price() const { return m_price; }
    Quantity total_volume() const { return m_total_volume; }
    std::size_t order_count() const { return m_orders.size(); }
    bool empty() const { return m_orders.empty(); }

    OrderList& orders() { return m_orders; }
    const OrderList& orders() const { return m_orders; }

    OrderIterator add_order(const Order& order) {
        m_orders.push_back(order);
        m_total_volume += order.remaining_quantity;
        OrderIterator it = m_orders.end();
        --it;
        return it;
    }

    void remove_order(OrderIterator it) {
        if (it != m_orders.end()) {
            m_total_volume -= it->remaining_quantity;
            m_orders.erase(it);
        }
    }

    void decrease_volume(Quantity qty) {
        if (qty > m_total_volume) {
            m_total_volume = 0;
        } else {
            m_total_volume -= qty;
        }
    }

private:
    Price m_price;
    Quantity m_total_volume;
    OrderList m_orders;
};

} // namespace lob

#endif // LOB_PRICE_LEVEL_HPP
