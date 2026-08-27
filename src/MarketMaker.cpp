#include "lob/MarketMaker.hpp"
#include <cmath>
#include <algorithm>

namespace lob {

MarketMaker::MarketMaker(const MarketMakerConfig& config)
    : m_config(config)
    , m_cash(config.initial_cash)
    , m_inventory(config.initial_inventory)
    , m_max_abs_inventory(std::abs(config.initial_inventory))
    , m_cost_basis(0.0)
    , m_realized_pnl(0.0)
    , m_active_bid_id(0)
    , m_active_ask_id(0)
    , m_next_order_id(config.strategy_id_start)
    , m_last_fair_value(0)
    , m_last_reservation_price(0.0)
    , m_last_bid_quote(0)
    , m_last_ask_quote(0)
    , m_orders_submitted(0)
    , m_orders_cancelled(0)
    , m_orders_filled(0)
    , m_buy_volume(0)
    , m_sell_volume(0)
{}

void MarketMaker::reset() {
    m_cash = m_config.initial_cash;
    m_inventory = m_config.initial_inventory;
    m_max_abs_inventory = std::abs(m_config.initial_inventory);
    m_cost_basis = 0.0;
    m_realized_pnl = 0.0;
    m_active_bid_id = 0;
    m_active_ask_id = 0;
    m_next_order_id = m_config.strategy_id_start;
    m_last_fair_value = 0;
    m_last_reservation_price = 0.0;
    m_last_bid_quote = 0;
    m_last_ask_quote = 0;
    m_orders_submitted = 0;
    m_orders_cancelled = 0;
    m_orders_filled = 0;
    m_buy_volume = 0;
    m_sell_volume = 0;
}

bool MarketMaker::calculate_fair_value(const OrderBook& book, Price& out_fair_value) const {
    Price best_b = 0, best_a = 0;
    Quantity vol_b = 0, vol_a = 0;
    if (book.get_best_bid(best_b, vol_b) && book.get_best_ask(best_a, vol_a)) {
        out_fair_value = static_cast<Price>(std::round(static_cast<double>(best_b + best_a) / 2.0));
        return true;
    }
    return false;
}

double MarketMaker::calculate_reservation_price(Price fair_value, int inventory) const {
    return static_cast<double>(fair_value) - (m_config.inventory_skew_factor * static_cast<double>(inventory));
}

void MarketMaker::on_market_events(const std::vector<MarketEvent>& events) {
    for (const auto& evt : events) {
        if (evt.event_type == EventType::OrderExecuted) {
            bool is_strategy_bid = (evt.order_id == m_active_bid_id);
            bool is_strategy_ask = (evt.order_id == m_active_ask_id);

            if (is_strategy_bid || is_strategy_ask) {
                m_orders_filled++;
                double fill_price = static_cast<double>(evt.price);
                double fill_qty = static_cast<double>(evt.quantity);

                if (evt.side == Side::Buy) {
                    m_buy_volume += evt.quantity;
                    m_cash -= (fill_qty * fill_price);

                    if (m_inventory >= 0) {
                        // Expanding long position
                        double cur_inv = static_cast<double>(m_inventory);
                        double new_inv = cur_inv + fill_qty;
                        m_cost_basis = (cur_inv * m_cost_basis + fill_qty * fill_price) / new_inv;
                        m_inventory += static_cast<int>(evt.quantity);
                    } else {
                        // Reducing short position
                        double cur_short = static_cast<double>(-m_inventory);
                        if (fill_qty <= cur_short) {
                            double pnl = fill_qty * (m_cost_basis - fill_price);
                            m_realized_pnl += pnl;
                            m_inventory += static_cast<int>(evt.quantity);
                            if (m_inventory == 0) {
                                m_cost_basis = 0.0;
                            }
                        } else {
                            // Overshooting short to long
                            double short_covered = cur_short;
                            double long_opened = fill_qty - cur_short;
                            double pnl = short_covered * (m_cost_basis - fill_price);
                            m_realized_pnl += pnl;
                            m_inventory = static_cast<int>(long_opened);
                            m_cost_basis = fill_price;
                        }
                    }

                    if (evt.remaining_quantity == 0 && is_strategy_bid) {
                        m_active_bid_id = 0;
                    }
                } else if (evt.side == Side::Sell) {
                    m_sell_volume += evt.quantity;
                    m_cash += (fill_qty * fill_price);

                    if (m_inventory <= 0) {
                        // Expanding short position
                        double cur_short = static_cast<double>(-m_inventory);
                        double new_short = cur_short + fill_qty;
                        m_cost_basis = (cur_short * m_cost_basis + fill_qty * fill_price) / new_short;
                        m_inventory -= static_cast<int>(evt.quantity);
                    } else {
                        // Reducing long position
                        double cur_long = static_cast<double>(m_inventory);
                        if (fill_qty <= cur_long) {
                            double pnl = fill_qty * (fill_price - m_cost_basis);
                            m_realized_pnl += pnl;
                            m_inventory -= static_cast<int>(evt.quantity);
                            if (m_inventory == 0) {
                                m_cost_basis = 0.0;
                            }
                        } else {
                            // Overshooting long to short
                            double long_closed = cur_long;
                            double short_opened = fill_qty - cur_long;
                            double pnl = long_closed * (fill_price - m_cost_basis);
                            m_realized_pnl += pnl;
                            m_inventory = -static_cast<int>(short_opened);
                            m_cost_basis = fill_price;
                        }
                    }

                    if (evt.remaining_quantity == 0 && is_strategy_ask) {
                        m_active_ask_id = 0;
                    }
                }

                m_max_abs_inventory = std::max(m_max_abs_inventory, std::abs(m_inventory));
            }
        } else if (evt.event_type == EventType::OrderCancelled) {
            if (evt.order_id == m_active_bid_id) {
                m_active_bid_id = 0;
            }
            if (evt.order_id == m_active_ask_id) {
                m_active_ask_id = 0;
            }
        }
    }
}

double MarketMaker::unrealized_pnl(Price mark_price) const {
    if (m_inventory == 0) {
        return 0.0;
    }
    double mark = static_cast<double>(mark_price);
    return static_cast<double>(m_inventory) * (mark - m_cost_basis);
}

double MarketMaker::total_pnl(Price mark_price) const {
    return m_realized_pnl + unrealized_pnl(mark_price);
}

void MarketMaker::cancel_active_quotes(MatchingEngine& engine, Timestamp ts) {
    if (m_active_bid_id != 0) {
        engine.cancel_order(m_active_bid_id, ts);
        m_active_bid_id = 0;
        m_orders_cancelled++;
    }
    if (m_active_ask_id != 0) {
        engine.cancel_order(m_active_ask_id, ts);
        m_active_ask_id = 0;
        m_orders_cancelled++;
    }
}

void MarketMaker::refresh_quotes(MatchingEngine& engine, Timestamp ts) {
    // 1. Cancel previous quotes
    cancel_active_quotes(engine, ts);

    // 2. Calculate Fair Value
    Price fv = 0;
    if (!calculate_fair_value(engine.book(), fv)) {
        return; // Book empty or one-sided; skip quoting
    }
    m_last_fair_value = fv;

    // 3. Calculate Reservation Price
    double R = calculate_reservation_price(fv, m_inventory);
    m_last_reservation_price = R;

    // 4. Calculate target Bid & Ask quotes
    Price target_bid = static_cast<Price>(std::round(R - static_cast<double>(m_config.base_half_spread)));
    Price target_ask = static_cast<Price>(std::round(R + static_cast<double>(m_config.base_half_spread)));

    if (target_bid >= target_ask) {
        target_ask = target_bid + 1;
    }
    if (target_bid <= 0) {
        target_bid = 1;
    }

    // Passive Quoting Bounds
    Price best_b = 0, best_a = 0;
    Quantity vol_b = 0, vol_a = 0;
    if (engine.book().get_best_ask(best_a, vol_a) && target_bid >= best_a) {
        target_bid = best_a - m_config.minimum_quote_distance;
    }
    if (engine.book().get_best_bid(best_b, vol_b) && target_ask <= best_b) {
        target_ask = best_b + m_config.minimum_quote_distance;
    }
    if (target_bid <= 0) {
        target_bid = 1;
    }

    // 5. Submit quotes subject to inventory limits
    if (m_inventory < m_config.max_inventory) {
        Quantity buy_qty = std::min(m_config.quote_size, static_cast<Quantity>(m_config.max_inventory - m_inventory));
        if (buy_qty > 0) {
            OrderId bid_id = get_next_strategy_order_id();
            auto events = engine.submit_limit_order(bid_id, Side::Buy, target_bid, buy_qty, ts);
            m_active_bid_id = bid_id;
            m_last_bid_quote = target_bid;
            m_orders_submitted++;
            on_market_events(events);
        }
    }

    if (m_inventory > -m_config.max_inventory) {
        Quantity sell_qty = std::min(m_config.quote_size, static_cast<Quantity>(m_config.max_inventory + m_inventory));
        if (sell_qty > 0) {
            OrderId ask_id = get_next_strategy_order_id();
            auto events = engine.submit_limit_order(ask_id, Side::Sell, target_ask, sell_qty, ts);
            m_active_ask_id = ask_id;
            m_last_ask_quote = target_ask;
            m_orders_submitted++;
            on_market_events(events);
        }
    }
}

} // namespace lob
