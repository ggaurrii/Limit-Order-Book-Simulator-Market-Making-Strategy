# Limit Order Book, Simulator & Quantitative Strategy Benchmarking (Phases 1–4)

High-performance, deterministic C++ Limit Order Book (LOB), Matching Engine, Synthetic Market Event Simulator, Baseline Inventory-Aware Market Maker, and Avellaneda-Stoikov Market Maker for quantitative trading research and strategy comparison.

> [!NOTE]
> This repository provides an experimental quantitative framework for backtesting and strategy benchmarking. It does not constitute financial advice or a production trading system.

## Features

- **Phase 1 — Core Matching Engine**:
  - **Price-Time Priority (FIFO)**: Queues per price level guarantee strict time priority.
  - **$O(1)$ Order Cancellation**: Hash map lookup combined with doubly-linked list nodes allows immediate $O(1)$ order cancellations.
  - **Fixed-Point Integer Pricing**: Prices represented in tick units (`int64_t`) to prevent floating-point rounding inaccuracies.
  - **Trade & Event Stream**: Generates deterministic execution trades, acceptances, cancellations, and rejections with sequence numbers.

- **Phase 2 — Synthetic Market Simulator**:
  - **Reproducible RNG (`std::mt19937_64`)**: Explicit random seed control (`seed = 42`) yields 100% deterministic event streams across runs.
  - **Configurable Market Regimes**: `NORMAL`, `HIGH_VOLATILITY`, `LOW_LIQUIDITY`, `BUY_IMBALANCE`, `SELL_IMBALANCE`.
  - **Evolving Reference Price Process**: Random walk with configurable volatility and directional drift parameters.

- **Phase 3 — Inventory-Aware Baseline Market Maker**:
  - **Fair Value & Reservation Price**: Fair value estimated from top-of-book mid-price $S = \frac{\text{Best Bid} + \text{Best Ask}}{2}$. Reservation price skews by inventory position: $R = S - (\gamma \times I)$.
  - **Inventory Limits (`max_inventory`)**: Suppresses buying when $I \ge \text{max\_inventory}$ and selling when $I \le -\text{max\_inventory}$. Caps quote quantities to respect boundaries.

- **Phase 4 — Avellaneda-Stoikov Market Maker & Strategy Comparison**:
  - **Avellaneda-Stoikov Framework**:
    - Reservation Price: $r = S - q \cdot \gamma \cdot \sigma^2 \cdot \tau$
    - Optimal Spread: $\delta = \frac{1}{\gamma} \ln\left(1 + \frac{\gamma}{\kappa}\right) + \frac{1}{2} \gamma \sigma^2 \tau$
  - **Causal Rolling Volatility Estimator**: Bounded rolling window of observed price returns (zero future information).
  - **Deterministic Event-Replay Harness**: Replays the exact same pre-generated external market event stream through both Baseline and Avellaneda-Stoikov strategies.
  - **Comprehensive Risk & Equity Metrics**: Sharpe ratio, maximum drawdown, PnL volatility, average absolute inventory, and fill rate.

## Directory Structure

```
limitorderbook/
├── CMakeLists.txt
├── Makefile
├── README.md
├── .gitignore
├── include/
│   └── lob/
│       ├── Types.hpp                   # Basic types, enums, fixed-point price
│       ├── Order.hpp                   # Order struct and node definitions
│       ├── PriceLevel.hpp              # Price level queue with total volume
│       ├── OrderBook.hpp               # Bid & Ask trees and ID lookup table
│       ├── MatchingEngine.hpp          # Order lifecycle & trade execution logic
│       ├── Trade.hpp                   # Execution trade record
│       ├── MarketEvent.hpp             # Order and market event definitions
│       ├── MarketRegime.hpp            # Market regime enums
│       ├── SimulatorConfig.hpp         # Simulator configuration parameters
│       ├── SimulatorEvent.hpp          # Generated event representation
│       ├── MarketSimulator.hpp         # Synthetic market event simulator
│       ├── MarketMakerConfig.hpp       # Baseline market maker config
│       ├── MarketMaker.hpp             # Baseline market-making strategy
│       ├── IMarketMakingStrategy.hpp   # Strategy interface
│       ├── AvellanedaStoikovConfig.hpp # Avellaneda-Stoikov strategy config
│       └── AvellanedaStoikovMarketMaker.hpp # Stochastic control market maker
├── src/
│   ├── OrderBook.cpp
│   ├── MatchingEngine.cpp
│   ├── MarketSimulator.cpp
│   ├── MarketMaker.cpp
│   ├── AvellanedaStoikovMarketMaker.cpp
│   ├── main.cpp                        # Interactive Phase 1 CLI demo
│   ├── sim_demo.cpp                    # Phase 2 Market Simulator integration demo
│   ├── market_maker_demo.cpp           # Phase 3 Market Maker strategy demo
│   └── strategy_comparison_demo.cpp   # Phase 4 Strategy Comparison demo
└── tests/
    ├── test_runner.hpp                 # Minimalist C++ unit test framework
    ├── test_order_book.cpp             # Unit tests for OrderBook data structures
    ├── test_matching_engine.cpp          # Unit tests for MatchingEngine rules
    ├── test_simulator.cpp             # Unit tests for MarketSimulator determinism & regimes
    ├── test_market_maker.cpp           # Unit tests for Baseline MarketMaker
    └── test_avellaneda_stoikov.cpp     # Unit tests for Avellaneda-Stoikov strategy
```

## Building and Testing

### Using `mingw32-make` / `g++` directly:

#### Compile & Run All 42 Unit Tests:
```bash
mingw32-make test
.\test_runner.exe
```

#### Run Strategy Comparison Benchmark (`strategy_comparison_demo.exe`):
```bash
mingw32-make strategy_comparison_demo.exe
.\strategy_comparison_demo.exe
```

#### Run Phase 3 Market Maker Strategy Demo:
```bash
mingw32-make market_maker_demo.exe
.\market_maker_demo.exe
```

#### Run Phase 2 Simulator Integration Demo:
```bash
mingw32-make sim_demo.exe
.\sim_demo.exe
```

### Using CMake:
```bash
mkdir build && cd build
cmake ..
cmake --build .
ctest --output-on-failure
```

## Strategy Benchmark Results (`strategy_comparison_demo.exe`)

Side-by-side performance under identical pre-generated market event streams (Seed `42`, 10,000 events):

```
========================================================================================
REGIME: HIGH_VOLATILITY (Seed: 42, Events: 10000)
========================================================================================
METRIC                     BASELINE (Phase 3)AVELLANEDA-STOIKOV (Phase 4)
----------------------------------------------------------------------------------------
Total PnL ($)                         8241.00                    19786.00
Realized PnL ($)                      8223.53                    19783.00
Unrealized PnL ($)                      17.47                        3.00
Sharpe Ratio                             0.17                        0.37
Max Drawdown ($)                     -3550.00                    -3866.00
PnL Volatility ($)                     497.30                      544.35
Final Inventory                           -13                          -3
Max Abs Inventory                          67                          20
Avg Abs Inventory                       14.71                        3.42
Inventory StdDev                        18.20                        5.80
Orders Submitted                         1964                        1962
Orders Filled                            1671                        1362
Fill Rate (%)                           85.08                       69.42
Traded Volume                           16667                       13597
========================================================================================

========================================================================================
REGIME: BUY_IMBALANCE (Seed: 42, Events: 10000)
========================================================================================
METRIC                     BASELINE (Phase 3)AVELLANEDA-STOIKOV (Phase 4)
----------------------------------------------------------------------------------------
Total PnL ($)                      -388939.00                    41846.00
Realized PnL ($)                   -383728.39                    47784.27
Unrealized PnL ($)                   -5210.61                    -5938.27
Sharpe Ratio                            -2.57                        0.12
Max Drawdown ($)                   -387149.00                   -79558.00
PnL Volatility ($)                    1537.92                     3787.19
Final Inventory                           -60                        -100
Max Abs Inventory                         100                         100
Avg Abs Inventory                       83.90                       57.33
Inventory StdDev                        18.59                       30.31
Orders Submitted                         1150                        1253
Orders Filled                             573                         575
Fill Rate (%)                           49.83                       45.89
Traded Volume                            5620                        5600
========================================================================================
```
