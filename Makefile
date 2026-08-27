CXX = g++
CXXFLAGS = -std=c++14 -O3 -Wall -Wextra -Wpedantic -Iinclude

SRCS = src/OrderBook.cpp src/MatchingEngine.cpp
TEST_SRCS = tests/test_order_book.cpp tests/test_matching_engine.cpp
MAIN_SRC = src/main.cpp

TEST_BIN = test_runner.exe
DEMO_BIN = lob_demo.exe

all: $(DEMO_BIN) $(TEST_BIN)

$(DEMO_BIN): $(SRCS) $(MAIN_SRC)
	$(CXX) $(CXXFLAGS) $(SRCS) $(MAIN_SRC) -o $(DEMO_BIN)

$(TEST_BIN): $(SRCS) $(TEST_SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) $(TEST_SRCS) -o $(TEST_BIN)

test: $(TEST_BIN)
	.\$(TEST_BIN)

clean:
	del /Q /F $(TEST_BIN) $(DEMO_BIN) *.o 2>NUL || clean

.PHONY: all test clean
