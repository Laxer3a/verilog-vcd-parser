# Makefile for multithreading tests

CXX         ?= g++
CXXFLAGS    += -I../src -I../build -std=c++11 -pthread -g -O2
LDFLAGS     += -pthread

BUILD_DIR   = ../build
LIB_FILE    = $(BUILD_DIR)/libverilog-vcd-parser.a

TEST_SRC    = test_multithread.cpp
TEST_BIN    = test_multithread

.PHONY: all clean test

all: $(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(LIB_FILE)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIB_FILE) $(LDFLAGS)

test: $(TEST_BIN)
	@echo "Running multithreading tests..."
	./$(TEST_BIN)

clean:
	rm -f $(TEST_BIN) test_vcd_*.vcd stress_test_*.vcd varsize_test_*.vcd reuse_test_*.vcd

help:
	@echo "Multithreading Test Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build the test executable"
	@echo "  test     - Build and run the tests"
	@echo "  clean    - Remove test binary and generated VCD files"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Prerequisites:"
	@echo "  - Build the main library first: cd .. && make"
