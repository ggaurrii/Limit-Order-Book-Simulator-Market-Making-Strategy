#include "lob/AvellanedaStoikovMarketMaker.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace lob {

AvellanedaStoikovMarketMaker::AvellanedaStoikovMarketMaker(const AvellanedaStoikovConfig& config)
    : m_config(config)
    , m_cash(config.initial_cash)
    , m_inventory(config.initial_inventory)
    , m_max_abs_inventory(std::abs(config.initial_inventory))
    , m_cost_basis(0.0)
    , m_realized_pnl(0.0)
    , m_returns_buffer(config.volatility_window, 0.0)
    , m_buffer_head(0)
    , m_buffer_count(0)
    , m_last_observed_mid(0)
    , m_cached_volatility(config.initial_volatility)
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

void AvellanedaStoikovMarketMaker::reset() {
    m_cash = m_config.initial_cash;
    m_inventory = m_config.initial_inventory;
    m_max_abs_inventory = std::abs(m_config.initial_inventory);
    m_cost_basis = 0.0;
    m_realized_pnl = 0.0;
    std::fill(m_returns_buffer.begin(), m_returns_buffer.end(), 0.0);
    m_buffer_head = 0;
    m_buffer_count = 0;
    m_last_observed_mid = 0;
    m_cached_volatility = m_config.initial_volatility;
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

bool AvellanedaStoikovMarketMaker::calculate_fair_value(const OrderBook& book, Price& out_fair_value) const {
    Price best_b = 0, best_a = 0;
    Quantity vol_b = 0, vol_a = 0;
    if (book.get_best_bid(best_b, vol_b) && book.get_best_ask(best_a, vol_a)) {
        out_fair_value = static_cast<Price>(std::round(static_cast<double>(best_b + best_a) / 2.0));
        return true;
    }
    return false;
}

void AvellanedaStoikovMarketMaker::update_volatility(Price mid_price) {
    if (m_last_observed_mid != 0 && mid_price != m_last_observed_mid) {
        double ret = static_cast<double>(mid_price - m_last_observed_mid);
        m_returns_buffer[m_buffer_head] = ret;
        m_buffer_head = (m_buffer_head + 1) % m_config.volatility_window;
        if (m_buffer_count < m_config.volatility_window) {
            m_buffer_count++;
        }
    }
    m_last_observed_mid = mid_price;

    if (m_buffer_count < 2) {
        m_cached_volatility = m_config.initial_volatility;
    } else {
        double sum = 0.0;
        for (std::size_t i = 0; i < m_buffer_count; ++i) {
            sum += m_returns_buffer[i];
        }
        double mean = sum / static_cast<double>(m_buffer_count);
        double sq_sum = 0.0;
        for (std::size_t i = 0; i < m_buffer_count; ++i) {
            double diff = m_returns_buffer[i] - mean;
            sq_sum += diff * diff;
        }
        double sample_std = std::sqrt(sq_sum / static_cast<double>(m_buffer_count - 1));
        m_cached_volatility = std::max(0.01, sample_std);
    }
}

double AvellanedaStoikovMarketMaker::current_volatility() const {
    return m_cached_volatility;
}

double AvellanedaStoikovMarketMaker::remaining_horizon_ratio(Timestamp ts) const {
    if (m_config.terminal_horizon == 0) {
        return 1.0;
    }
    double elapsed = static_cast<double>(ts);
    double total = static_cast<double>(m_config.terminal_horizon);
    double ratio = std::max(0.0, 1.0 - (elapsed / total));
    return ratio;
}

double AvellanedaStoikovMarketMaker::calculate_reservation_price(Price fair_value, int inventory, double sigma, double tau) const {
    double S = static_cast<double>(fair_value);
    double q = static_cast<double>(inventory);
    double g = m_config.gamma;
    return S - (q * g * sigma * sigma * tau);
}

double AvellanedaStoikovMarketMaker::calculate_optimal_spread(double sigma, double tau) const {
    double g = m_config.gamma;
    double k = m_config.kappa;
    double term1 = (1.0 / g) * std::log(1.0 + (g / k));
    double term2 = 0.5 * g * sigma * sigma * tau;
    return term1 + term2;
}

void AvellanedaStoikovMarketMaker::on_market_events(const std::vector<MarketEvent>& events) {
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
                        double cur_inv = static_cast<double>(m_inventory);
                        double new_inv = cur_inv + fill_qty;
                        m_cost_basis = (cur_inv * m_cost_basis + fill_qty * fill_price) / new_inv;
                        m_inventory += static_cast<int>(evt.quantity);
                    } else {
                        double cur_short = static_cast<double>(-m_inventory);
                        if (fill_qty <= cur_short) {
                            double pnl = fill_qty * (m_cost_basis - fill_price);
                            m_realized_pnl += pnl;
                            m_inventory += static_cast<int>(evt.quantity);
                            if (m_inventory == 0) {
                                m_cost_basis = 0.0;
                            }
                        } else {
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
                        double cur_short = static_cast<double>(-m_inventory);
                        double new_short = cur_short + fill_qty;
                        m_cost_basis = (cur_short * m_cost_basis + fill_qty * fill_price) / new_short;
                        m_inventory -= static_cast<int>(evt.quantity);
                    } else {
                        double cur_long = static_cast<double>(m_inventory);
                        if (fill_qty <= cur_long) {
                            double pnl = fill_qty * (fill_price - m_cost_basis);
                            m_realized_pnl += pnl;
                            m_inventory -= static_cast<int>(evt.quantity);
                            if (m_inventory == 0) {
                                m_cost_basis = 0.0;
                            }
                        } else {
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

double AvellanedaStoikovMarketMaker::unrealized_pnl(Price mark_price) const {
    if (m_inventory == 0) {
        return 0.0;
    }
    double mark = static_cast<double>(mark_price);
    return static_cast<double>(m_inventory) * (mark - m_cost_basis);
}

double AvellanedaStoikovMarketMaker::total_pnl(Price mark_price) const {
    return m_realized_pnl + unrealized_pnl(mark_price);
}

void AvellanedaStoikovMarketMaker::cancel_active_quotes(MatchingEngine& engine, Timestamp ts) {
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

void AvellanedaStoikovMarketMaker::refresh_quotes(MatchingEngine& engine, Timestamp ts) {
    // 1. Cancel previous quotes
    cancel_active_quotes(engine, ts);

    // 2. Calculate Fair Value
    Price fv = 0;
    if (!calculate_fair_value(engine.book(), fv)) {
        return; // Book empty or one-sided; skip quoting
    }

    // 3. Update rolling volatility & time-to-horizon
    update_volatility(fv);
    double sigma = current_volatility();
    double tau = remaining_horizon_ratio(ts);

    // 4. Calculate Reservation Price & Optimal Spread
    double r = calculate_reservation_price(fv, m_inventory, sigma, tau);
    double delta = calculate_optimal_spread(sigma, tau);

    m_last_fair_value = fv;
    m_last_reservation_price = r;

    // 5. Target Quotes
    Price target_bid = static_cast<Price>(std::floor(r - delta));
    Price target_ask = static_cast<Price>(std::ceil(r + delta));

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

    // 6. Submit quotes subject to inventory limits
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
