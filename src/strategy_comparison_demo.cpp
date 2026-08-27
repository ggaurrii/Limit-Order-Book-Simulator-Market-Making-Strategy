#include "lob/MarketSimulator.hpp"
#include "lob/MatchingEngine.hpp"
#include "lob/MarketMaker.hpp"
#include "lob/AvellanedaStoikovMarketMaker.hpp"

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <numeric>
#include <string>

struct Snapshot {
    lob::Timestamp timestamp;
    double cash;
    int inventory;
    lob::Price mark_price;
    double total_pnl;
};

struct StrategyMetrics {
    std::string strategy_name;
    double initial_cash;
    double final_cash;
    int final_inventory;
    int max_abs_inventory;
    double avg_abs_inventory;
    double inventory_stddev;
    double realized_pnl;
    double unrealized_pnl;
    double total_pnl;
    double max_drawdown;
    double pnl_volatility;
    double sharpe_ratio;
    std::size_t orders_submitted;
    std::size_t orders_cancelled;
    std::size_t orders_filled;
    double fill_rate;
    lob::Quantity buy_volume;
    lob::Quantity sell_volume;
    lob::Price final_fair_value;
    double final_reservation_price;
    lob::Price final_bid_quote;
    lob::Price final_ask_quote;
};

StrategyMetrics run_strategy_replay(
    lob::IMarketMakingStrategy& strategy,
    const std::vector<lob::SimulatorEvent>& external_events,
    std::size_t refresh_interval,
    lob::Price initial_ref_price)
{
    lob::MatchingEngine engine;
    strategy.reset();

    std::vector<Snapshot> snapshots;

    // Warm up book with first 50 events
    std::size_t N = external_events.size();
    std::size_t start_idx = std::min(static_cast<std::size_t>(50), N);

    for (std::size_t i = 0; i < start_idx; ++i) {
        const auto& sim_evt = external_events[i];
        if (sim_evt.action == lob::ActionType::SubmitLimit) {
            engine.submit_limit_order(sim_evt.order_id, sim_evt.side, sim_evt.price, sim_evt.quantity, sim_evt.timestamp);
        } else if (sim_evt.action == lob::ActionType::SubmitMarket) {
            engine.submit_market_order(sim_evt.order_id, sim_evt.side, sim_evt.quantity, sim_evt.timestamp);
        } else if (sim_evt.action == lob::ActionType::Cancel) {
            engine.cancel_order(sim_evt.order_id, sim_evt.timestamp);
        }
    }

    // Main replay loop
    for (std::size_t i = start_idx; i < N; ++i) {
        const auto& sim_evt = external_events[i];
        std::vector<lob::MarketEvent> engine_events;

        if (sim_evt.action == lob::ActionType::SubmitLimit) {
            engine_events = engine.submit_limit_order(sim_evt.order_id, sim_evt.side, sim_evt.price, sim_evt.quantity, sim_evt.timestamp);
        } else if (sim_evt.action == lob::ActionType::SubmitMarket) {
            engine_events = engine.submit_market_order(sim_evt.order_id, sim_evt.side, sim_evt.quantity, sim_evt.timestamp);
        } else if (sim_evt.action == lob::ActionType::Cancel) {
            engine_events = engine.cancel_order(sim_evt.order_id, sim_evt.timestamp);
        }

        // Strategy reacts strictly to past events
        strategy.on_market_events(engine_events);

        if (i % refresh_interval == 0) {
            strategy.refresh_quotes(engine, sim_evt.timestamp);
        }

        // Snapshot equity every 100 events
        if (i % 100 == 0) {
            lob::Price mark = 0;
            if (strategy.fair_value() != 0) {
                mark = strategy.fair_value();
            } else {
                double mid = 0.0;
                if (engine.book().get_mid_price(mid)) {
                    mark = static_cast<lob::Price>(std::round(mid));
                } else {
                    mark = initial_ref_price;
                }
            }
            snapshots.push_back(Snapshot{
                sim_evt.timestamp,
                strategy.cash(),
                strategy.inventory(),
                mark,
                strategy.total_pnl(mark)
            });
        }
    }

    lob::Price final_mark = 0;
    if (strategy.fair_value() != 0) {
        final_mark = strategy.fair_value();
    } else {
        double mid = 0.0;
        if (engine.book().get_mid_price(mid)) {
            final_mark = static_cast<lob::Price>(std::round(mid));
        } else {
            final_mark = initial_ref_price;
        }
    }

    // Calculate Risk & PnL Metrics
    double init_cash = strategy.cash() - strategy.realized_pnl();
    double final_cash = strategy.cash();
    int final_inv = strategy.inventory();
    double r_pnl = strategy.realized_pnl();
    double u_pnl = strategy.unrealized_pnl(final_mark);
    double tot_pnl = strategy.total_pnl(final_mark);

    // Snapshot time series statistics
    double peak = 0.0;
    double max_dd = 0.0;
    std::vector<double> periodic_returns;
    std::vector<double> abs_inventories;
    std::vector<double> inventories;

    for (std::size_t k = 0; k < snapshots.size(); ++k) {
        double pnl = snapshots[k].total_pnl;
        peak = std::max(peak, pnl);
        double dd = pnl - peak;
        max_dd = std::min(max_dd, dd);

        if (k > 0) {
            periodic_returns.push_back(pnl - snapshots[k - 1].total_pnl);
        }
        abs_inventories.push_back(std::abs(snapshots[k].inventory));
        inventories.push_back(static_cast<double>(snapshots[k].inventory));
    }

    double sum_abs_inv = std::accumulate(abs_inventories.begin(), abs_inventories.end(), 0.0);
    double avg_abs_inv = abs_inventories.empty() ? 0.0 : (sum_abs_inv / abs_inventories.size());

    double inv_mean = std::accumulate(inventories.begin(), inventories.end(), 0.0) / (inventories.empty() ? 1.0 : inventories.size());
    double inv_sq_sum = 0.0;
    for (double inv : inventories) {
        inv_sq_sum += (inv - inv_mean) * (inv - inv_mean);
    }
    double inv_stddev = inventories.size() > 1 ? std::sqrt(inv_sq_sum / (inventories.size() - 1)) : 0.0;

    // Periodic Return Mean & StdDev
    double mean_ret = 0.0;
    double stddev_ret = 0.0;
    if (!periodic_returns.empty()) {
        double sum_ret = std::accumulate(periodic_returns.begin(), periodic_returns.end(), 0.0);
        mean_ret = sum_ret / periodic_returns.size();
        double ret_sq_sum = 0.0;
        for (double r : periodic_returns) {
            ret_sq_sum += (r - mean_ret) * (r - mean_ret);
        }
        stddev_ret = (periodic_returns.size() > 1) ? std::sqrt(ret_sq_sum / (periodic_returns.size() - 1)) : 0.0;
    }

    double sharpe = (stddev_ret > 1e-9) ? (mean_ret / stddev_ret) : 0.0;

    double fill_rate = (strategy.orders_submitted() > 0) ? (static_cast<double>(strategy.orders_filled()) / strategy.orders_submitted()) * 100.0 : 0.0;

    return StrategyMetrics{
        "", init_cash, final_cash, final_inv, strategy.max_abs_inventory(), avg_abs_inv, inv_stddev,
        r_pnl, u_pnl, tot_pnl, max_dd, stddev_ret, sharpe,
        strategy.orders_submitted(), strategy.orders_cancelled(), strategy.orders_filled(),
        fill_rate, strategy.buy_volume(), strategy.sell_volume(),
        strategy.fair_value(), strategy.reservation_price(), strategy.current_bid_quote(), strategy.current_ask_quote()
    };
}

void compare_regime(lob::MarketRegime regime, std::uint64_t seed, std::size_t num_events) {
    // 1. Generate external event stream ONCE
    lob::SimulatorConfig sim_cfg = lob::get_regime_config(regime);
    lob::MarketSimulator sim(sim_cfg, seed);

    std::vector<lob::SimulatorEvent> events;
    events.reserve(num_events);
    for (std::size_t i = 0; i < num_events; ++i) {
        events.push_back(sim.next_event());
    }

    // 2. Instantiate strategies with aligned parameters
    lob::MarketMakerConfig baseline_cfg;
    baseline_cfg.quote_size = 10;
    baseline_cfg.base_half_spread = 2;
    baseline_cfg.max_inventory = 100;
    baseline_cfg.inventory_skew_factor = 0.1;
    baseline_cfg.quote_refresh_interval = 10;
    baseline_cfg.initial_cash = 1000000.0;
    lob::MarketMaker baseline_strategy(baseline_cfg);

    lob::AvellanedaStoikovConfig as_cfg;
    as_cfg.quote_size = 10;
    as_cfg.gamma = 0.1;
    as_cfg.kappa = 1.5;
    as_cfg.max_inventory = 100;
    as_cfg.quote_refresh_interval = 10;
    as_cfg.initial_cash = 1000000.0;
    as_cfg.terminal_horizon = num_events * sim_cfg.time_step_ns;
    lob::AvellanedaStoikovMarketMaker as_strategy(as_cfg);

    // 3. Replay exact same stream through both strategies
    StrategyMetrics base_m = run_strategy_replay(baseline_strategy, events, baseline_cfg.quote_refresh_interval, sim_cfg.initial_reference_price);
    base_m.strategy_name = "BASELINE";

    StrategyMetrics as_m = run_strategy_replay(as_strategy, events, as_cfg.quote_refresh_interval, sim_cfg.initial_reference_price);
    as_m.strategy_name = "AVELLANEDA_STOIKOV";

    std::cout << "\n========================================================================================\n";
    std::cout << "REGIME: " << lob::to_string(regime) << " (Seed: " << seed << ", Events: " << num_events << ")\n";
    std::cout << "========================================================================================\n";
    std::cout << std::left << std::setw(20) << "METRIC"
              << std::right << std::setw(25) << "BASELINE (Phase 3)"
              << std::right << std::setw(28) << "AVELLANEDA-STOIKOV (Phase 4)" << "\n";
    std::cout << "----------------------------------------------------------------------------------------\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::left << std::setw(20) << "Total PnL ($)"
              << std::right << std::setw(25) << base_m.total_pnl
              << std::right << std::setw(28) << as_m.total_pnl << "\n";
    std::cout << std::left << std::setw(20) << "Realized PnL ($)"
              << std::right << std::setw(25) << base_m.realized_pnl
              << std::right << std::setw(28) << as_m.realized_pnl << "\n";
    std::cout << std::left << std::setw(20) << "Unrealized PnL ($)"
              << std::right << std::setw(25) << base_m.unrealized_pnl
              << std::right << std::setw(28) << as_m.unrealized_pnl << "\n";
    std::cout << std::left << std::setw(20) << "Sharpe Ratio"
              << std::right << std::setw(25) << base_m.sharpe_ratio
              << std::right << std::setw(28) << as_m.sharpe_ratio << "\n";
    std::cout << std::left << std::setw(20) << "Max Drawdown ($)"
              << std::right << std::setw(25) << base_m.max_drawdown
              << std::right << std::setw(28) << as_m.max_drawdown << "\n";
    std::cout << std::left << std::setw(20) << "PnL Volatility ($)"
              << std::right << std::setw(25) << base_m.pnl_volatility
              << std::right << std::setw(28) << as_m.pnl_volatility << "\n";
    std::cout << std::left << std::setw(20) << "Final Inventory"
              << std::right << std::setw(25) << base_m.final_inventory
              << std::right << std::setw(28) << as_m.final_inventory << "\n";
    std::cout << std::left << std::setw(20) << "Max Abs Inventory"
              << std::right << std::setw(25) << base_m.max_abs_inventory
              << std::right << std::setw(28) << as_m.max_abs_inventory << "\n";
    std::cout << std::left << std::setw(20) << "Avg Abs Inventory"
              << std::right << std::setw(25) << base_m.avg_abs_inventory
              << std::right << std::setw(28) << as_m.avg_abs_inventory << "\n";
    std::cout << std::left << std::setw(20) << "Inventory StdDev"
              << std::right << std::setw(25) << base_m.inventory_stddev
              << std::right << std::setw(28) << as_m.inventory_stddev << "\n";
    std::cout << std::left << std::setw(20) << "Orders Submitted"
              << std::right << std::setw(25) << base_m.orders_submitted
              << std::right << std::setw(28) << as_m.orders_submitted << "\n";
    std::cout << std::left << std::setw(20) << "Orders Filled"
              << std::right << std::setw(25) << base_m.orders_filled
              << std::right << std::setw(28) << as_m.orders_filled << "\n";
    std::cout << std::left << std::setw(20) << "Fill Rate (%)"
              << std::right << std::setw(25) << base_m.fill_rate
              << std::right << std::setw(28) << as_m.fill_rate << "\n";
    std::cout << std::left << std::setw(20) << "Traded Volume"
              << std::right << std::setw(25) << (base_m.buy_volume + base_m.sell_volume)
              << std::right << std::setw(28) << (as_m.buy_volume + as_m.sell_volume) << "\n";
    std::cout << "========================================================================================\n";
}

int main() {
    std::cout << "Quantitative Strategy Comparison: Baseline vs. Avellaneda-Stoikov (Phase 4)\n";

    std::uint64_t seed = 42;
    std::size_t num_events = 10000;

    compare_regime(lob::MarketRegime::Normal, seed, num_events);
    compare_regime(lob::MarketRegime::HighVolatility, seed, num_events);
    compare_regime(lob::MarketRegime::LowLiquidity, seed, num_events);
    compare_regime(lob::MarketRegime::BuyImbalance, seed, num_events);
    compare_regime(lob::MarketRegime::SellImbalance, seed, num_events);

    return 0;
}
