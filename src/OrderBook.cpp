#include "lob/OrderBook.hpp"

namespace lob {

bool OrderBook::add_order(const Order& order) {
    if (order.id == 0 || order.remaining_quantity == 0) {
        return false;
    }
    if (m_order_map.find(order.id) != m_order_map.end()) {
        return false; // Duplicate OrderId rejected
    }

    if (order.side == Side::Buy) {
        auto map_it = m_bids.find(order.price);
        if (map_it == m_bids.end()) {
            auto res = m_bids.emplace(order.price, PriceLevel(order.price));
            map_it = res.first;
        }
        auto list_it = map_it->second.add_order(order);
        m_order_map[order.id] = OrderLocation{Side::Buy, order.price, list_it};
    } else {
        auto map_it = m_asks.find(order.price);
        if (map_it == m_asks.end()) {
            auto res = m_asks.emplace(order.price, PriceLevel(order.price));
            map_it = res.first;
        }
        auto list_it = map_it->second.add_order(order);
        m_order_map[order.id] = OrderLocation{Side::Sell, order.price, list_it};
    }

    return true;
}

bool OrderBook::cancel_order(OrderId order_id) {
    Order temp(0, Side::Buy, OrderType::Limit, 0, 0, 0);
    return cancel_order(order_id, temp);
}

bool OrderBook::cancel_order(OrderId order_id, Order& out_cancelled_order) {
    auto map_it = m_order_map.find(order_id);
    if (map_it == m_order_map.end()) {
        return false;
    }

    const OrderLocation& loc = map_it->second;

    if (loc.side == Side::Buy) {
        auto bid_it = m_bids.find(loc.price);
        if (bid_it != m_bids.end()) {
            out_cancelled_order = *(loc.iterator);
            bid_it->second.remove_order(loc.iterator);
            if (bid_it->second.empty()) {
                m_bids.erase(bid_it);
            }
        }
    } else {
        auto ask_it = m_asks.find(loc.price);
        if (ask_it != m_asks.end()) {
            out_cancelled_order = *(loc.iterator);
            ask_it->second.remove_order(loc.iterator);
            if (ask_it->second.empty()) {
                m_asks.erase(ask_it);
            }
        }
    }

    m_order_map.erase(map_it);
    return true;
}

bool OrderBook::has_order(OrderId order_id) const {
    return m_order_map.find(order_id) != m_order_map.end();
}

const Order* OrderBook::find_order(OrderId order_id) const {
    auto map_it = m_order_map.find(order_id);
    if (map_it == m_order_map.end()) {
        return nullptr;
    }
    return &(*(map_it->second.iterator));
}

bool OrderBook::get_best_bid(Price& price, Quantity& volume) const {
    if (m_bids.empty()) {
        return false;
    }
    price = m_bids.begin()->first;
    volume = m_bids.begin()->second.total_volume();
    return true;
}

bool OrderBook::get_best_ask(Price& price, Quantity& volume) const {
    if (m_asks.empty()) {
        return false;
    }
    price = m_asks.begin()->first;
    volume = m_asks.begin()->second.total_volume();
    return true;
}

bool OrderBook::get_spread(Price& spread) const {
    Price best_b = 0, best_a = 0;
    Quantity vol_b = 0, vol_a = 0;
    if (get_best_bid(best_b, vol_b) && get_best_ask(best_a, vol_a)) {
        spread = best_a - best_b;
        return true;
    }
    return false;
}

bool OrderBook::get_mid_price(double& mid_price) const {
    Price best_b = 0, best_a = 0;
    Quantity vol_b = 0, vol_a = 0;
    if (get_best_bid(best_b, vol_b) && get_best_ask(best_a, vol_a)) {
        mid_price = static_cast<double>(best_b + best_a) / 2.0;
        return true;
    }
    return false;
}

std::vector<LevelInfo> OrderBook::get_bid_depth(std::size_t levels) const {
    std::vector<LevelInfo> result;
    result.reserve(levels);
    std::size_t count = 0;
    for (auto it = m_bids.begin(); it != m_bids.end() && count < levels; ++it, ++count) {
        result.push_back(LevelInfo{it->first, it->second.total_volume(), it->second.order_count()});
    }
    return result;
}

std::vector<LevelInfo> OrderBook::get_ask_depth(std::size_t levels) const {
    std::vector<LevelInfo> result;
    result.reserve(levels);
    std::size_t count = 0;
    for (auto it = m_asks.begin(); it != m_asks.end() && count < levels; ++it, ++count) {
        result.push_back(LevelInfo{it->first, it->second.total_volume(), it->second.order_count()});
    }
    return result;
}

void OrderBook::clear() {
    m_bids.clear();
    m_asks.clear();
    m_order_map.clear();
}

} // namespace lob
