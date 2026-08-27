CXX = g++
CXXFLAGS = -std=c++14 -O3 -Wall -Wextra -Wpedantic -Iinclude

SRCS = src/OrderBook.cpp src/MatchingEngine.cpp src/MarketSimulator.cpp src/MarketMaker.cpp src/AvellanedaStoikovMarketMaker.cpp
TEST_SRCS = tests/test_order_book.cpp tests/test_matching_engine.cpp tests/test_simulator.cpp tests/test_market_maker.cpp tests/test_avellaneda_stoikov.cpp
MAIN_SRC = src/main.cpp
SIM_SRC = src/sim_demo.cpp
MM_SRC = src/market_maker_demo.cpp
COMP_SRC = src/strategy_comparison_demo.cpp

TEST_BIN = test_runner.exe
DEMO_BIN = lob_demo.exe
SIM_BIN = sim_demo.exe
MM_BIN = market_maker_demo.exe
COMP_BIN = strategy_comparison_demo.exe

all: $(DEMO_BIN) $(SIM_BIN) $(MM_BIN) $(COMP_BIN) $(TEST_BIN)

$(DEMO_BIN): $(SRCS) $(MAIN_SRC)
	$(CXX) $(CXXFLAGS) $(SRCS) $(MAIN_SRC) -o $(DEMO_BIN)

$(SIM_BIN): $(SRCS) $(SIM_SRC)
	$(CXX) $(CXXFLAGS) $(SRCS) $(SIM_SRC) -o $(SIM_BIN)

$(MM_BIN): $(SRCS) $(MM_SRC)
	$(CXX) $(CXXFLAGS) $(SRCS) $(MM_SRC) -o $(MM_BIN)

$(COMP_BIN): $(SRCS) $(COMP_SRC)
	$(CXX) $(CXXFLAGS) $(SRCS) $(COMP_SRC) -o $(COMP_BIN)

$(TEST_BIN): $(SRCS) $(TEST_SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) $(TEST_SRCS) -o $(TEST_BIN)

test: $(TEST_BIN)
	.\$(TEST_BIN)

clean:
	del /Q /F $(TEST_BIN) $(DEMO_BIN) $(SIM_BIN) $(MM_BIN) $(COMP_BIN) *.o 2>NUL || clean

.PHONY: all test clean
