#include "test_runner.hpp"
#include "lob/MarketSimulator.hpp"
#include "lob/MatchingEngine.hpp"

#include <vector>
#include <cmath>

using namespace lob;

TEST(SimulatorTest, Determinism) {
    std::uint64_t seed = 42;
    MarketSimulator sim1(MarketRegime::Normal, seed);
    MarketSimulator sim2(MarketRegime::Normal, seed);

    const std::size_t num_events = 1000;
    for (std::size_t i = 0; i < num_events; ++i) {
        SimulatorEvent ev1 = sim1.next_event();
        SimulatorEvent ev2 = sim2.next_event();

        EXPECT_EQ(ev1.action, ev2.action);
        EXPECT_EQ(ev1.order_id, ev2.order_id);
        EXPECT_EQ(ev1.side, ev2.side);
        EXPECT_EQ(ev1.order_type, ev2.order_type);
        EXPECT_EQ(ev1.price, ev2.price);
        EXPECT_EQ(ev1.quantity, ev2.quantity);
        EXPECT_EQ(ev1.timestamp, ev2.timestamp);
    }
}

TEST(SimulatorTest, SeedVariation) {
    MarketSimulator sim1(MarketRegime::Normal, 42);
    MarketSimulator sim2(MarketRegime::Normal, 43);

    bool difference_found = false;
    for (std::size_t i = 0; i < 100; ++i) {
        SimulatorEvent ev1 = sim1.next_event();
        SimulatorEvent ev2 = sim2.next_event();

        if (ev1.price != ev2.price || ev1.quantity != ev2.quantity || ev1.action != ev2.action) {
            difference_found = true;
            break;
        }
    }
    EXPECT_TRUE(difference_found);
}

TEST(SimulatorTest, EventValidity) {
    MarketSimulator sim(MarketRegime::Normal, 42);

    for (std::size_t i = 0; i < 1000; ++i) {
        SimulatorEvent ev = sim.next_event();
        EXPECT_TRUE(ev.order_id > 0);
        EXPECT_TRUE(ev.timestamp > 0);

        if (ev.action == ActionType::SubmitLimit) {
            EXPECT_TRUE(ev.price > 0);
            EXPECT_TRUE(ev.quantity > 0);
        } else if (ev.action == ActionType::SubmitMarket) {
            EXPECT_TRUE(ev.quantity > 0);
        }
    }
}

TEST(SimulatorTest, EventDistributionFrequencies) {
    SimulatorConfig config;
    config.limit_buy_prob = 0.40;
    config.limit_sell_prob = 0.40;
    config.market_buy_prob = 0.05;
    config.market_sell_prob = 0.05;
    config.cancel_prob = 0.10;

    MarketSimulator sim(config, 100);

    const std::size_t N = 10000;
    std::size_t limit_buys = 0;
    std::size_t limit_sells = 0;
    std::size_t market_buys = 0;
    std::size_t market_sells = 0;
    std::size_t cancels = 0;

    for (std::size_t i = 0; i < N; ++i) {
        SimulatorEvent ev = sim.next_event();
        if (ev.action == ActionType::SubmitLimit) {
            if (ev.side == Side::Buy) limit_buys++;
            else limit_sells++;
        } else if (ev.action == ActionType::SubmitMarket) {
            if (ev.side == Side::Buy) market_buys++;
            else market_sells++;
        } else {
            cancels++;
        }
    }

    double emp_lb = static_cast<double>(limit_buys) / N;
    double emp_ls = static_cast<double>(limit_sells) / N;
    double emp_mb = static_cast<double>(market_buys) / N;
    double emp_ms = static_cast<double>(market_sells) / N;
    double emp_cnl = static_cast<double>(cancels) / N;

    // Tolerance of +/- 2.0% for statistical sampling
    EXPECT_TRUE(std::abs(emp_lb - 0.40) < 0.02);
    EXPECT_TRUE(std::abs(emp_ls - 0.40) < 0.02);
    EXPECT_TRUE(std::abs(emp_mb - 0.05) < 0.02);
    EXPECT_TRUE(std::abs(emp_ms - 0.05) < 0.02);
    EXPECT_TRUE(std::abs(emp_cnl - 0.10) < 0.02);
}

TEST(SimulatorTest, CancellationValidity) {
    MarketSimulator sim(MarketRegime::Normal, 42);

    for (std::size_t i = 0; i < 2000; ++i) {
        SimulatorEvent ev = sim.next_event();
        if (ev.action == ActionType::Cancel) {
            EXPECT_TRUE(ev.order_id > 0);
        }
    }
}

TEST(SimulatorTest, RegimeVariations) {
    MarketSimulator sim_norm(MarketRegime::Normal, 42);
    MarketSimulator sim_vol(MarketRegime::HighVolatility, 42);
    MarketSimulator sim_buy(MarketRegime::BuyImbalance, 42);

    std::size_t norm_markets = 0, vol_markets = 0, buy_market_buys = 0;

    for (std::size_t i = 0; i < 1000; ++i) {
        SimulatorEvent ev_n = sim_norm.next_event();
        SimulatorEvent ev_v = sim_vol.next_event();
        SimulatorEvent ev_b = sim_buy.next_event();

        if (ev_n.action == ActionType::SubmitMarket) norm_markets++;
        if (ev_v.action == ActionType::SubmitMarket) vol_markets++;
        if (ev_b.action == ActionType::SubmitMarket && ev_b.side == Side::Buy) buy_market_buys++;
    }

    // High Volatility should produce significantly more market orders than Normal
    EXPECT_TRUE(vol_markets > norm_markets);

    // Buy Imbalance should produce strong buy market order presence
    EXPECT_TRUE(buy_market_buys > 150);
}

TEST(SimulatorTest, ReferencePriceProcess) {
    MarketSimulator sim(MarketRegime::BuyImbalance, 42);
    Price init_ref = sim.reference_price();

    for (std::size_t i = 0; i < 1000; ++i) {
        sim.next_event();
        EXPECT_TRUE(sim.reference_price() > 0);
    }

    // Buy imbalance positive drift should drive reference price higher
    EXPECT_TRUE(sim.reference_price() > init_ref);
}

TEST(SimulatorTest, EngineIntegrationStream) {
    MarketSimulator sim(MarketRegime::Normal, 42);
    MatchingEngine engine;

    std::size_t total_trades = 0;
    for (std::size_t i = 0; i < 5000; ++i) {
        auto engine_events = sim.process_next_event(engine);
        for (const auto& evt : engine_events) {
            if (evt.event_type == EventType::TradeGenerated) {
                total_trades++;
            }
        }
    }

    EXPECT_TRUE(total_trades > 0);
    EXPECT_TRUE(engine.book().total_order_count() > 0);
}
