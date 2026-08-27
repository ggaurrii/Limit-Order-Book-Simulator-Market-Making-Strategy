#include "lob/MatchingEngine.hpp"
#include <algorithm>

namespace lob {

std::vector<MarketEvent> MatchingEngine::submit_limit_order(OrderId id, Side side, Price price, Quantity qty, Timestamp ts) {
    return submit_order(Order(id, side, OrderType::Limit, price, qty, ts));
}

std::vector<MarketEvent> MatchingEngine::submit_market_order(OrderId id, Side side, Quantity qty, Timestamp ts) {
    return submit_order(Order(id, side, OrderType::Market, 0, qty, ts));
}

std::vector<MarketEvent> MatchingEngine::submit_order(Order order) {
    std::vector<MarketEvent> events;

    // 1. Validation
    if (order.id == 0) {
        MarketEvent rej;
        rej.event_type = EventType::OrderRejected;
        rej.sequence_number = get_next_sequence();
        rej.timestamp = order.timestamp;
        rej.order_id = order.id;
        rej.side = order.side;
        rej.price = order.price;
        rej.quantity = order.initial_quantity;
        rej.remaining_quantity = order.initial_quantity;
        rej.status = OrderStatus::Rejected;
        rej.reason = "Invalid OrderId (must be > 0)";
        events.push_back(rej);
        return events;
    }

    if (order.initial_quantity == 0) {
        MarketEvent rej;
        rej.event_type = EventType::OrderRejected;
        rej.sequence_number = get_next_sequence();
        rej.timestamp = order.timestamp;
        rej.order_id = order.id;
        rej.side = order.side;
        rej.price = order.price;
        rej.quantity = order.initial_quantity;
        rej.remaining_quantity = 0;
        rej.status = OrderStatus::Rejected;
        rej.reason = "Invalid quantity (must be > 0)";
        events.push_back(rej);
        return events;
    }

    if (m_book.has_order(order.id)) {
        MarketEvent rej;
        rej.event_type = EventType::OrderRejected;
        rej.sequence_number = get_next_sequence();
        rej.timestamp = order.timestamp;
        rej.order_id = order.id;
        rej.side = order.side;
        rej.price = order.price;
        rej.quantity = order.initial_quantity;
        rej.remaining_quantity = order.initial_quantity;
        rej.status = OrderStatus::Rejected;
        rej.reason = "Duplicate OrderId";
        events.push_back(rej);
        return events;
    }

    if (order.type == OrderType::Limit && order.price <= 0) {
        MarketEvent rej;
        rej.event_type = EventType::OrderRejected;
        rej.sequence_number = get_next_sequence();
        rej.timestamp = order.timestamp;
        rej.order_id = order.id;
        rej.side = order.side;
        rej.price = order.price;
        rej.quantity = order.initial_quantity;
        rej.remaining_quantity = order.initial_quantity;
        rej.status = OrderStatus::Rejected;
        rej.reason = "Invalid limit price (must be > 0)";
        events.push_back(rej);
        return events;
    }

    // 2. Accept Order
    MarketEvent acc;
    acc.event_type = EventType::OrderAccepted;
    acc.sequence_number = get_next_sequence();
    acc.timestamp = order.timestamp;
    acc.order_id = order.id;
    acc.side = order.side;
    acc.price = order.price;
    acc.quantity = order.initial_quantity;
    acc.remaining_quantity = order.initial_quantity;
    acc.status = OrderStatus::New;
    events.push_back(acc);

    // 3. Match Order
    std::vector<MarketEvent> match_events;
    if (order.type == OrderType::Limit) {
        if (order.side == Side::Buy) {
            match_events = match_limit_buy(order);
        } else {
            match_events = match_limit_sell(order);
        }
    } else { // Market
        if (order.side == Side::Buy) {
            match_events = match_market_buy(order);
        } else {
            match_events = match_market_sell(order);
        }
    }

    events.insert(events.end(), match_events.begin(), match_events.end());
    return events;
}

std::vector<MarketEvent> MatchingEngine::match_limit_buy(Order& buy_order) {
    std::vector<MarketEvent> events;

    while (buy_order.remaining_quantity > 0 && !m_book.ask_empty()) {
        auto ask_it = m_book.asks().begin();
        Price best_ask_price = ask_it->first;

        if (buy_order.price < best_ask_price) {
            break; // No match possible
        }

        PriceLevel& ask_level = ask_it->second;

        while (buy_order.remaining_quantity > 0 && !ask_level.empty()) {
            Order& maker_order = ask_level.orders().front();
            Quantity fill_qty = std::min(buy_order.remaining_quantity, maker_order.remaining_quantity);
            Price trade_price = maker_order.price;

            buy_order.remaining_quantity -= fill_qty;
            maker_order.remaining_quantity -= fill_qty;
            ask_level.decrease_volume(fill_qty);

            Trade trade{
                get_next_trade_id(),
                buy_order.id,
                maker_order.id,
                trade_price,
                fill_qty,
                buy_order.timestamp,
                Side::Sell
            };

            // Trade Generated Event
            MarketEvent trd_evt;
            trd_evt.event_type = EventType::TradeGenerated;
            trd_evt.sequence_number = get_next_sequence();
            trd_evt.timestamp = buy_order.timestamp;
            trd_evt.order_id = buy_order.id;
            trd_evt.side = buy_order.side;
            trd_evt.price = trade_price;
            trd_evt.quantity = fill_qty;
            trd_evt.remaining_quantity = buy_order.remaining_quantity;
            trd_evt.status = buy_order.is_filled() ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
            trd_evt.trade = trade;
            events.push_back(trd_evt);

            // Taker fill event
            MarketEvent taker_exec;
            taker_exec.event_type = EventType::OrderExecuted;
            taker_exec.sequence_number = get_next_sequence();
            taker_exec.timestamp = buy_order.timestamp;
            taker_exec.order_id = buy_order.id;
            taker_exec.side = buy_order.side;
            taker_exec.price = trade_price;
            taker_exec.quantity = fill_qty;
            taker_exec.remaining_quantity = buy_order.remaining_quantity;
            taker_exec.status = buy_order.is_filled() ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
            taker_exec.trade = trade;
            events.push_back(taker_exec);

            // Maker fill event
            MarketEvent maker_exec;
            maker_exec.event_type = EventType::OrderExecuted;
            maker_exec.sequence_number = get_next_sequence();
            maker_exec.timestamp = buy_order.timestamp;
            maker_exec.order_id = maker_order.id;
            maker_exec.side = maker_order.side;
            maker_exec.price = trade_price;
            maker_exec.quantity = fill_qty;
            maker_exec.remaining_quantity = maker_order.remaining_quantity;
            maker_exec.status = maker_order.is_filled() ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
            maker_exec.trade = trade;
            events.push_back(maker_exec);

            if (maker_order.is_filled()) {
                m_book.order_map().erase(maker_order.id);
                ask_level.orders().pop_front();
            }
        }

        if (ask_level.empty()) {
            m_book.asks().erase(ask_it);
        }
    }

    if (buy_order.remaining_quantity > 0) {
        m_book.add_order(buy_order);
    }

    return events;
}

std::vector<MarketEvent> MatchingEngine::match_limit_sell(Order& sell_order) {
    std::vector<MarketEvent> events;

    while (sell_order.remaining_quantity > 0 && !m_book.bid_empty()) {
        auto bid_it = m_book.bids().begin();
        Price best_bid_price = bid_it->first;

        if (sell_order.price > best_bid_price) {
            break; // No match possible
        }

        PriceLevel& bid_level = bid_it->second;

        while (sell_order.remaining_quantity > 0 && !bid_level.empty()) {
            Order& maker_order = bid_level.orders().front();
            Quantity fill_qty = std::min(sell_order.remaining_quantity, maker_order.remaining_quantity);
            Price trade_price = maker_order.price;

            sell_order.remaining_quantity -= fill_qty;
            maker_order.remaining_quantity -= fill_qty;
            bid_level.decrease_volume(fill_qty);

            Trade trade{
                get_next_trade_id(),
                maker_order.id,
                sell_order.id,
                trade_price,
                fill_qty,
                sell_order.timestamp,
                Side::Buy
            };

            // Trade Generated Event
            MarketEvent trd_evt;
            trd_evt.event_type = EventType::TradeGenerated;
            trd_evt.sequence_number = get_next_sequence();
            trd_evt.timestamp = sell_order.timestamp;
            trd_evt.order_id = sell_order.id;
            trd_evt.side = sell_order.side;
            trd_evt.price = trade_price;
            trd_evt.quantity = fill_qty;
            trd_evt.remaining_quantity = sell_order.remaining_quantity;
            trd_evt.status = sell_order.is_filled() ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
            trd_evt.trade = trade;
            events.push_back(trd_evt);

            // Taker fill event
            MarketEvent taker_exec;
            taker_exec.event_type = EventType::OrderExecuted;
            taker_exec.sequence_number = get_next_sequence();
            taker_exec.timestamp = sell_order.timestamp;
            taker_exec.order_id = sell_order.id;
            taker_exec.side = sell_order.side;
            taker_exec.price = trade_price;
            taker_exec.quantity = fill_qty;
            taker_exec.remaining_quantity = sell_order.remaining_quantity;
            taker_exec.status = sell_order.is_filled() ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
            taker_exec.trade = trade;
            events.push_back(taker_exec);

            // Maker fill event
            MarketEvent maker_exec;
            maker_exec.event_type = EventType::OrderExecuted;
            maker_exec.sequence_number = get_next_sequence();
            maker_exec.timestamp = sell_order.timestamp;
            maker_exec.order_id = maker_order.id;
            maker_exec.side = maker_order.side;
            maker_exec.price = trade_price;
            maker_exec.quantity = fill_qty;
            maker_exec.remaining_quantity = maker_order.remaining_quantity;
            maker_exec.status = maker_order.is_filled() ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
            maker_exec.trade = trade;
            events.push_back(maker_exec);

            if (maker_order.is_filled()) {
                m_book.order_map().erase(maker_order.id);
                bid_level.orders().pop_front();
            }
        }

        if (bid_level.empty()) {
            m_book.bids().erase(bid_it);
        }
    }

    if (sell_order.remaining_quantity > 0) {
        m_book.add_order(sell_order);
    }

    return events;
}

std::vector<MarketEvent> MatchingEngine::match_market_buy(Order& buy_order) {
    std::vector<MarketEvent> events;

    while (buy_order.remaining_quantity > 0 && !m_book.ask_empty()) {
        auto ask_it = m_book.asks().begin();
        PriceLevel& ask_level = ask_it->second;

        while (buy_order.remaining_quantity > 0 && !ask_level.empty()) {
            Order& maker_order = ask_level.orders().front();
            Quantity fill_qty = std::min(buy_order.remaining_quantity, maker_order.remaining_quantity);
            Price trade_price = maker_order.price;

            buy_order.remaining_quantity -= fill_qty;
            maker_order.remaining_quantity -= fill_qty;
            ask_level.decrease_volume(fill_qty);

            Trade trade{
                get_next_trade_id(),
                buy_order.id,
                maker_order.id,
                trade_price,
                fill_qty,
                buy_order.timestamp,
                Side::Sell
            };

            // Trade Generated Event
            MarketEvent trd_evt;
            trd_evt.event_type = EventType::TradeGenerated;
            trd_evt.sequence_number = get_next_sequence();
            trd_evt.timestamp = buy_order.timestamp;
            trd_evt.order_id = buy_order.id;
            trd_evt.side = buy_order.side;
            trd_evt.price = trade_price;
            trd_evt.quantity = fill_qty;
            trd_evt.remaining_quantity = buy_order.remaining_quantity;
            trd_evt.status = buy_order.is_filled() ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
            trd_evt.trade = trade;
            events.push_back(trd_evt);

            // Taker fill event
            MarketEvent taker_exec;
            taker_exec.event_type = EventType::OrderExecuted;
            taker_exec.sequence_number = get_next_sequence();
            taker_exec.timestamp = buy_order.timestamp;
            taker_exec.order_id = buy_order.id;
            taker_exec.side = buy_order.side;
            taker_exec.price = trade_price;
            taker_exec.quantity = fill_qty;
            taker_exec.remaining_quantity = buy_order.remaining_quantity;
            taker_exec.status = buy_order.is_filled() ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
            taker_exec.trade = trade;
            events.push_back(taker_exec);

            // Maker fill event
            MarketEvent maker_exec;
            maker_exec.event_type = EventType::OrderExecuted;
            maker_exec.sequence_number = get_next_sequence();
            maker_exec.timestamp = buy_order.timestamp;
            maker_exec.order_id = maker_order.id;
            maker_exec.side = maker_order.side;
            maker_exec.price = trade_price;
            maker_exec.quantity = fill_qty;
            maker_exec.remaining_quantity = maker_order.remaining_quantity;
            maker_exec.status = maker_order.is_filled() ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
            maker_exec.trade = trade;
            events.push_back(maker_exec);

            if (maker_order.is_filled()) {
                m_book.order_map().erase(maker_order.id);
                ask_level.orders().pop_front();
            }
        }

        if (ask_level.empty()) {
            m_book.asks().erase(ask_it);
        }
    }

    // Cancel any unfilled market order quantity
    if (buy_order.remaining_quantity > 0) {
        MarketEvent cnl;
        cnl.event_type = EventType::OrderCancelled;
        cnl.sequence_number = get_next_sequence();
        cnl.timestamp = buy_order.timestamp;
        cnl.order_id = buy_order.id;
        cnl.side = buy_order.side;
        cnl.price = 0;
        cnl.quantity = buy_order.remaining_quantity;
        cnl.remaining_quantity = 0;
        cnl.status = OrderStatus::Cancelled;
        cnl.reason = "Unfilled market order quantity cancelled";
        events.push_back(cnl);
    }

    return events;
}

std::vector<MarketEvent> MatchingEngine::match_market_sell(Order& sell_order) {
    std::vector<MarketEvent> events;

    while (sell_order.remaining_quantity > 0 && !m_book.bid_empty()) {
        auto bid_it = m_book.bids().begin();
        PriceLevel& bid_level = bid_it->second;

        while (sell_order.remaining_quantity > 0 && !bid_level.empty()) {
            Order& maker_order = bid_level.orders().front();
            Quantity fill_qty = std::min(sell_order.remaining_quantity, maker_order.remaining_quantity);
            Price trade_price = maker_order.price;

            sell_order.remaining_quantity -= fill_qty;
            maker_order.remaining_quantity -= fill_qty;
            bid_level.decrease_volume(fill_qty);

            Trade trade{
                get_next_trade_id(),
                maker_order.id,
                sell_order.id,
                trade_price,
                fill_qty,
                sell_order.timestamp,
                Side::Buy
            };

            // Trade Generated Event
            MarketEvent trd_evt;
            trd_evt.event_type = EventType::TradeGenerated;
            trd_evt.sequence_number = get_next_sequence();
            trd_evt.timestamp = sell_order.timestamp;
            trd_evt.order_id = sell_order.id;
            trd_evt.side = sell_order.side;
            trd_evt.price = trade_price;
            trd_evt.quantity = fill_qty;
            trd_evt.remaining_quantity = sell_order.remaining_quantity;
            trd_evt.status = sell_order.is_filled() ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
            trd_evt.trade = trade;
            events.push_back(trd_evt);

            // Taker fill event
            MarketEvent taker_exec;
            taker_exec.event_type = EventType::OrderExecuted;
            taker_exec.sequence_number = get_next_sequence();
            taker_exec.timestamp = sell_order.timestamp;
            taker_exec.order_id = sell_order.id;
            taker_exec.side = sell_order.side;
            taker_exec.price = trade_price;
            taker_exec.quantity = fill_qty;
            taker_exec.remaining_quantity = sell_order.remaining_quantity;
            taker_exec.status = sell_order.is_filled() ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
            taker_exec.trade = trade;
            events.push_back(taker_exec);

            // Maker fill event
            MarketEvent maker_exec;
            maker_exec.event_type = EventType::OrderExecuted;
            maker_exec.sequence_number = get_next_sequence();
            maker_exec.timestamp = sell_order.timestamp;
            maker_exec.order_id = maker_order.id;
            maker_exec.side = maker_order.side;
            maker_exec.price = trade_price;
            maker_exec.quantity = fill_qty;
            maker_exec.remaining_quantity = maker_order.remaining_quantity;
            maker_exec.status = maker_order.is_filled() ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
            maker_exec.trade = trade;
            events.push_back(maker_exec);

            if (maker_order.is_filled()) {
                m_book.order_map().erase(maker_order.id);
                bid_level.orders().pop_front();
            }
        }

        if (bid_level.empty()) {
            m_book.bids().erase(bid_it);
        }
    }

    // Cancel any unfilled market order quantity
    if (sell_order.remaining_quantity > 0) {
        MarketEvent cnl;
        cnl.event_type = EventType::OrderCancelled;
        cnl.sequence_number = get_next_sequence();
        cnl.timestamp = sell_order.timestamp;
        cnl.order_id = sell_order.id;
        cnl.side = sell_order.side;
        cnl.price = 0;
        cnl.quantity = sell_order.remaining_quantity;
        cnl.remaining_quantity = 0;
        cnl.status = OrderStatus::Cancelled;
        cnl.reason = "Unfilled market order quantity cancelled";
        events.push_back(cnl);
    }

    return events;
}

std::vector<MarketEvent> MatchingEngine::cancel_order(OrderId id, Timestamp ts) {
    std::vector<MarketEvent> events;

    Order cancelled_order(0, Side::Buy, OrderType::Limit, 0, 0, 0);
    if (!m_book.cancel_order(id, cancelled_order)) {
        MarketEvent rej;
        rej.event_type = EventType::OrderRejected;
        rej.sequence_number = get_next_sequence();
        rej.timestamp = ts;
        rej.order_id = id;
        rej.side = Side::Buy;
        rej.price = 0;
        rej.quantity = 0;
        rej.remaining_quantity = 0;
        rej.status = OrderStatus::Rejected;
        rej.reason = "Order ID not found for cancellation";
        events.push_back(rej);
        return events;
    }

    MarketEvent cnl;
    cnl.event_type = EventType::OrderCancelled;
    cnl.sequence_number = get_next_sequence();
    cnl.timestamp = ts;
    cnl.order_id = cancelled_order.id;
    cnl.side = cancelled_order.side;
    cnl.price = cancelled_order.price;
    cnl.quantity = cancelled_order.remaining_quantity;
    cnl.remaining_quantity = 0;
    cnl.status = OrderStatus::Cancelled;
    events.push_back(cnl);

    return events;
}

void MatchingEngine::reset() {
    m_book.clear();
    m_next_sequence = 1;
    m_next_trade_id = 1;
}

} // namespace lob
