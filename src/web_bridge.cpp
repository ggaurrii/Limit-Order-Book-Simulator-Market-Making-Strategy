#include "lob/MatchingEngine.hpp"
#include "lob/MarketSimulator.hpp"
#include "lob/MarketMaker.hpp"
#include "lob/AvellanedaStoikovMarketMaker.hpp"
#include "lob/Types.hpp"

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <deque>
#include <algorithm>
#include <iomanip>
#include <cmath>

namespace {

enum class MMStrategyType {
    None,
    Baseline,
    AvellanedaStoikov
};

std::string escape_json_str(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '"') o << "\\\"";
        else if (c == '\\') o << "\\\\";
        else if (c == '\b') o << "\\b";
        else if (c == '\f') o << "\\f";
        else if (c == '\n') o << "\\n";
        else if (c == '\r') o << "\\r";
        else if (c == '\t') o << "\\t";
        else if (static_cast<unsigned char>(c) <= 0x1f) {
            o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
        } else {
            o << c;
        }
    }
    return o.str();
}

} // namespace

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    lob::MatchingEngine engine;
    lob::MarketSimulator simulator(lob::MarketRegime::Normal, 42);
    lob::MarketMaker baseline_mm;
    lob::AvellanedaStoikovMarketMaker as_mm;

    MMStrategyType active_strategy = MMStrategyType::None;

    std::deque<lob::Trade> recent_trades;
    std::vector<lob::OrderId> user_active_order_ids;

    lob::OrderId next_user_order_id = 900000;
    lob::Timestamp current_timestamp = 1000;

    // Helper lambda to get mark price
    auto get_mark_price = [&]() -> lob::Price {
        double mid = 0.0;
        if (engine.book().get_mid_price(mid)) {
            return static_cast<lob::Price>(std::round(mid));
        }
        return simulator.reference_price();
    };

    // Helper to serialize full state to JSON
    auto dump_state_json = [&](const std::string& status_msg = "ok") {
        lob::Price best_bid = 0, best_ask = 0, spread = 0;
        lob::Quantity bid_vol = 0, ask_vol = 0;
        double mid_price = 0.0;

        bool has_bid = engine.book().get_best_bid(best_bid, bid_vol);
        bool has_ask = engine.book().get_best_ask(best_ask, ask_vol);
        bool has_spread = engine.book().get_spread(spread);
        bool has_mid = engine.book().get_mid_price(mid_price);

        auto bid_depth = engine.book().get_bid_depth(15);
        auto ask_depth = engine.book().get_ask_depth(15);

        lob::Price mark_price = get_mark_price();

        // Filter valid user open orders
        std::vector<lob::Order> user_orders;
        std::vector<lob::OrderId> updated_user_active_ids;
        for (auto id : user_active_order_ids) {
            const lob::Order* ord = engine.book().find_order(id);
            if (ord) {
                user_orders.push_back(*ord);
                updated_user_active_ids.push_back(id);
            }
        }
        user_active_order_ids = std::move(updated_user_active_ids);

        std::cout << "{\"type\":\"STATE\",\"status\":\"" << escape_json_str(status_msg) << "\"";
        std::cout << ",\"sequence\":" << engine.current_sequence();
        std::cout << ",\"trade_id_seq\":" << engine.current_trade_id();
        std::cout << ",\"ref_price\":" << simulator.reference_price();
        std::cout << ",\"has_bid\":" << (has_bid ? "true" : "false");
        std::cout << ",\"best_bid\":" << best_bid;
        std::cout << ",\"best_bid_vol\":" << bid_vol;
        std::cout << ",\"has_ask\":" << (has_ask ? "true" : "false");
        std::cout << ",\"best_ask\":" << best_ask;
        std::cout << ",\"best_ask_vol\":" << ask_vol;
        std::cout << ",\"has_spread\":" << (has_spread ? "true" : "false");
        std::cout << ",\"spread\":" << spread;
        std::cout << ",\"has_mid\":" << (has_mid ? "true" : "false");
        std::cout << ",\"mid_price\":" << std::fixed << std::setprecision(2) << mid_price;
        std::cout << ",\"total_orders\":" << engine.book().total_order_count();

        // Bids
        std::cout << ",\"bids\":[";
        for (std::size_t i = 0; i < bid_depth.size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << "{\"price\":" << bid_depth[i].price 
                      << ",\"volume\":" << bid_depth[i].volume 
                      << ",\"count\":" << bid_depth[i].order_count << "}";
        }
        std::cout << "]";

        // Asks
        std::cout << ",\"asks\":[";
        for (std::size_t i = 0; i < ask_depth.size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << "{\"price\":" << ask_depth[i].price 
                      << ",\"volume\":" << ask_depth[i].volume 
                      << ",\"count\":" << ask_depth[i].order_count << "}";
        }
        std::cout << "]";

        // Recent Trades
        std::cout << ",\"trades\":[";
        for (std::size_t i = 0; i < recent_trades.size(); ++i) {
            if (i > 0) std::cout << ",";
            const auto& t = recent_trades[i];
            std::string taker_side = (t.maker_side == lob::Side::Buy) ? "SELL" : "BUY";
            std::cout << "{\"trade_id\":" << t.trade_id
                      << ",\"buyer_id\":" << t.buy_order_id
                      << ",\"seller_id\":" << t.sell_order_id
                      << ",\"side\":\"" << taker_side << "\""
                      << ",\"price\":" << t.price
                      << ",\"quantity\":" << t.quantity
                      << ",\"timestamp\":" << t.timestamp << "}";
        }
        std::cout << "]";

        // User Open Orders
        std::cout << ",\"user_orders\":[";
        for (std::size_t i = 0; i < user_orders.size(); ++i) {
            if (i > 0) std::cout << ",";
            const auto& uo = user_orders[i];
            std::cout << "{\"order_id\":" << uo.id
                      << ",\"side\":\"" << (uo.side == lob::Side::Buy ? "BUY" : "SELL") << "\""
                      << ",\"price\":" << uo.price
                      << ",\"quantity\":" << uo.remaining_quantity
                      << ",\"type\":\"LIMIT\""
                      << ",\"timestamp\":" << uo.timestamp << "}";
        }
        std::cout << "]";

        // Market Maker Stats
        std::cout << ",\"mm\":{";
        if (active_strategy == MMStrategyType::Baseline) {
            std::cout << "\"strategy\":\"BASELINE\""
                      << ",\"inventory\":" << baseline_mm.inventory()
                      << ",\"cash\":" << std::fixed << std::setprecision(2) << baseline_mm.cash()
                      << ",\"realized_pnl\":" << baseline_mm.realized_pnl()
                      << ",\"unrealized_pnl\":" << baseline_mm.unrealized_pnl(mark_price)
                      << ",\"total_pnl\":" << baseline_mm.total_pnl(mark_price)
                      << ",\"reservation_price\":" << baseline_mm.reservation_price()
                      << ",\"optimal_spread\":" << (baseline_mm.config().base_half_spread * 2)
                      << ",\"volatility\":0.0"
                      << ",\"active_bid\":" << baseline_mm.active_bid_id()
                      << ",\"active_ask\":" << baseline_mm.active_ask_id()
                      << ",\"orders_submitted\":" << baseline_mm.orders_submitted()
                      << ",\"orders_filled\":" << baseline_mm.orders_filled();
        } else if (active_strategy == MMStrategyType::AvellanedaStoikov) {
            std::cout << "\"strategy\":\"AVELLANEDA_STOIKOV\""
                      << ",\"inventory\":" << as_mm.inventory()
                      << ",\"cash\":" << std::fixed << std::setprecision(2) << as_mm.cash()
                      << ",\"realized_pnl\":" << as_mm.realized_pnl()
                      << ",\"unrealized_pnl\":" << as_mm.unrealized_pnl(mark_price)
                      << ",\"total_pnl\":" << as_mm.total_pnl(mark_price)
                      << ",\"reservation_price\":" << as_mm.reservation_price()
                      << ",\"optimal_spread\":" << (as_mm.current_ask_quote() - as_mm.current_bid_quote())
                      << ",\"volatility\":" << as_mm.current_volatility()
                      << ",\"active_bid\":" << as_mm.active_bid_id()
                      << ",\"active_ask\":" << as_mm.active_ask_id()
                      << ",\"orders_submitted\":" << as_mm.orders_submitted()
                      << ",\"orders_filled\":" << as_mm.orders_filled();
        } else {
            std::cout << "\"strategy\":\"NONE\""
                      << ",\"inventory\":0"
                      << ",\"cash\":0.0"
                      << ",\"realized_pnl\":0.0"
                      << ",\"unrealized_pnl\":0.0"
                      << ",\"total_pnl\":0.0"
                      << ",\"reservation_price\":0.0"
                      << ",\"optimal_spread\":0.0"
                      << ",\"volatility\":0.0"
                      << ",\"active_bid\":0"
                      << ",\"active_ask\":0"
                      << ",\"orders_submitted\":0"
                      << ",\"orders_filled\":0";
        }
        std::cout << "}";

        std::cout << "}\n" << std::flush;
    };

    // Main line-based command loop
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        current_timestamp += 10;

        if (cmd == "STATE" || cmd == "GET_STATE") {
            dump_state_json("ok");
        } else if (cmd == "RESET") {
            engine.reset();
            simulator = lob::MarketSimulator(lob::MarketRegime::Normal, 42);
            baseline_mm.reset();
            as_mm.reset();
            active_strategy = MMStrategyType::None;
            recent_trades.clear();
            user_active_order_ids.clear();
            dump_state_json("reset_complete");
        } else if (cmd == "SET_MM") {
            std::string strat_name;
            iss >> strat_name;
            if (strat_name == "BASELINE") {
                if (active_strategy == MMStrategyType::AvellanedaStoikov) {
                    as_mm.cancel_active_quotes(engine, current_timestamp);
                }
                active_strategy = MMStrategyType::Baseline;
            } else if (strat_name == "AVELLANEDA_STOIKOV" || strat_name == "AS") {
                if (active_strategy == MMStrategyType::Baseline) {
                    baseline_mm.cancel_active_quotes(engine, current_timestamp);
                }
                active_strategy = MMStrategyType::AvellanedaStoikov;
            } else {
                if (active_strategy == MMStrategyType::Baseline) {
                    baseline_mm.cancel_active_quotes(engine, current_timestamp);
                } else if (active_strategy == MMStrategyType::AvellanedaStoikov) {
                    as_mm.cancel_active_quotes(engine, current_timestamp);
                }
                active_strategy = MMStrategyType::None;
            }
            dump_state_json("mm_updated");
        } else if (cmd == "SET_REGIME") {
            std::string regime_str;
            iss >> regime_str;
            lob::MarketRegime reg = lob::MarketRegime::Normal;
            if (regime_str == "HIGH_VOLATILITY") reg = lob::MarketRegime::HighVolatility;
            else if (regime_str == "LOW_LIQUIDITY") reg = lob::MarketRegime::LowLiquidity;
            else if (regime_str == "BUY_IMBALANCE") reg = lob::MarketRegime::BuyImbalance;
            else if (regime_str == "SELL_IMBALANCE") reg = lob::MarketRegime::SellImbalance;

            simulator.set_regime(reg);
            dump_state_json("regime_updated");
        } else if (cmd == "SIM_STEP") {
            int count = 1;
            iss >> count;
            if (count < 1) count = 1;
            if (count > 500) count = 500;

            for (int i = 0; i < count; ++i) {
                lob::SimulatorEvent sim_evt = simulator.next_event();
                std::vector<lob::MarketEvent> evts;

                if (sim_evt.action == lob::ActionType::SubmitLimit) {
                    evts = engine.submit_limit_order(sim_evt.order_id, sim_evt.side, sim_evt.price, sim_evt.quantity, current_timestamp);
                } else if (sim_evt.action == lob::ActionType::SubmitMarket) {
                    evts = engine.submit_market_order(sim_evt.order_id, sim_evt.side, sim_evt.quantity, current_timestamp);
                } else if (sim_evt.action == lob::ActionType::Cancel) {
                    evts = engine.cancel_order(sim_evt.order_id, current_timestamp);
                }

                // Process trades
                for (const auto& evt : evts) {
                    if (evt.event_type == lob::EventType::TradeGenerated) {
                        recent_trades.push_front(evt.trade);
                        if (recent_trades.size() > 50) recent_trades.pop_back();
                    }
                }

                // Update market makers if enabled
                if (active_strategy == MMStrategyType::Baseline) {
                    baseline_mm.on_market_events(evts);
                    baseline_mm.refresh_quotes(engine, current_timestamp);
                } else if (active_strategy == MMStrategyType::AvellanedaStoikov) {
                    as_mm.on_market_events(evts);
                    as_mm.refresh_quotes(engine, current_timestamp);
                }
            }
            dump_state_json("sim_step_done");
        } else if (cmd == "SUBMIT_LIMIT") {
            std::string side_str;
            lob::Price price;
            lob::Quantity qty;
            iss >> side_str >> price >> qty;

            lob::Side side = (side_str == "BUY" || side_str == "buy") ? lob::Side::Buy : lob::Side::Sell;
            lob::OrderId user_oid = next_user_order_id++;

            auto evts = engine.submit_limit_order(user_oid, side, price, qty, current_timestamp);
            user_active_order_ids.push_back(user_oid);

            for (const auto& evt : evts) {
                if (evt.event_type == lob::EventType::TradeGenerated) {
                    recent_trades.push_front(evt.trade);
                    if (recent_trades.size() > 50) recent_trades.pop_back();
                }
            }

            if (active_strategy == MMStrategyType::Baseline) {
                baseline_mm.on_market_events(evts);
                baseline_mm.refresh_quotes(engine, current_timestamp);
            } else if (active_strategy == MMStrategyType::AvellanedaStoikov) {
                as_mm.on_market_events(evts);
                as_mm.refresh_quotes(engine, current_timestamp);
            }

            dump_state_json("order_submitted");
        } else if (cmd == "SUBMIT_MARKET") {
            std::string side_str;
            lob::Quantity qty;
            iss >> side_str >> qty;

            lob::Side side = (side_str == "BUY" || side_str == "buy") ? lob::Side::Buy : lob::Side::Sell;
            lob::OrderId user_oid = next_user_order_id++;

            auto evts = engine.submit_market_order(user_oid, side, qty, current_timestamp);

            for (const auto& evt : evts) {
                if (evt.event_type == lob::EventType::TradeGenerated) {
                    recent_trades.push_front(evt.trade);
                    if (recent_trades.size() > 50) recent_trades.pop_back();
                }
            }

            if (active_strategy == MMStrategyType::Baseline) {
                baseline_mm.on_market_events(evts);
                baseline_mm.refresh_quotes(engine, current_timestamp);
            } else if (active_strategy == MMStrategyType::AvellanedaStoikov) {
                as_mm.on_market_events(evts);
                as_mm.refresh_quotes(engine, current_timestamp);
            }

            dump_state_json("market_order_executed");
        } else if (cmd == "CANCEL") {
            lob::OrderId cancel_id;
            iss >> cancel_id;

            auto evts = engine.cancel_order(cancel_id, current_timestamp);
            user_active_order_ids.erase(
                std::remove(user_active_order_ids.begin(), user_active_order_ids.end(), cancel_id),
                user_active_order_ids.end()
            );

            if (active_strategy == MMStrategyType::Baseline) {
                baseline_mm.on_market_events(evts);
                baseline_mm.refresh_quotes(engine, current_timestamp);
            } else if (active_strategy == MMStrategyType::AvellanedaStoikov) {
                as_mm.on_market_events(evts);
                as_mm.refresh_quotes(engine, current_timestamp);
            }

            dump_state_json("order_cancelled");
        } else {
            dump_state_json("unknown_command");
        }
    }

    return 0;
}
