CXX      := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -I./lib -I./include
LDFLAGS  :=

TEST_SRC    := tests/main.cpp tests/smcparser_test.cpp tests/unit_test.cpp
TEST_LIB    := lib/catch_amalgamated.cpp
PARSER_SRC  := src/parsers/smcparser.cpp
ALGO_SRC    := src/algorithms/stalta.cpp

TEST_BIN := tests_runner

.PHONY: all clean test

all: $(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(TEST_LIB) $(PARSER_SRC) $(ALGO_SRC)
	$(CXX) $(CXXFLAGS) $(TEST_LIB) $(PARSER_SRC) $(ALGO_SRC) $(TEST_SRC) -o $(TEST_BIN) $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(TEST_BIN)


