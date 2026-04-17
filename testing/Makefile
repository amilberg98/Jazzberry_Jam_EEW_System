CXX      := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -I./lib -I./include -I../include
LDFLAGS  :=

TEST_SRC    := tests/main.cpp tests/smcparser_test.cpp tests/unit_test.cpp
TEST_LIB    := lib/catch_amalgamated.cpp
PARSER_SRC  := src/parsers/smcparser.cpp
FW_ALGO_SRC := ../src/STALTA.cpp ../src/taucpd.cpp ../src/biquad.cpp
UTIL_SRC    := src/smc_replay_util.cpp

TEST_BIN := tests_runner
UTIL_BIN := smc_replay_util

.PHONY: all clean test util replay

all: $(TEST_BIN) $(UTIL_BIN)

$(TEST_BIN): $(TEST_SRC) $(TEST_LIB) $(PARSER_SRC) $(FW_ALGO_SRC)
	$(CXX) $(CXXFLAGS) $(TEST_LIB) $(PARSER_SRC) $(FW_ALGO_SRC) $(TEST_SRC) -o $(TEST_BIN) $(LDFLAGS)

$(UTIL_BIN): $(UTIL_SRC) $(PARSER_SRC) $(FW_ALGO_SRC)
	$(CXX) $(CXXFLAGS) $(PARSER_SRC) $(FW_ALGO_SRC) $(UTIL_SRC) -o $(UTIL_BIN) $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

util: $(UTIL_BIN)

replay: $(UTIL_BIN)
	./$(UTIL_BIN) data/20021103_2212_uncorr

clean:
	rm -f $(TEST_BIN) $(UTIL_BIN)


