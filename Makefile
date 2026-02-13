# RespiroSync Build System
# Makefile for building core library and tests

CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pedantic
INCLUDES = -I.

# Core library
CORE_SRC = core/respirosync_core.cpp
CORE_OBJ = core/respirosync_core.o
CORE_LIB = librespirosync.a

# Tests
TEST_SRC = tests/test_core.cpp
TEST_BIN = tests/test_core

.PHONY: all clean test help

all: $(CORE_LIB) test

# Build core library
$(CORE_OBJ): $(CORE_SRC) core/respirosync_core.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $(CORE_SRC) -o $(CORE_OBJ)

$(CORE_LIB): $(CORE_OBJ)
	ar rcs $(CORE_LIB) $(CORE_OBJ)

# Build and run tests
test: $(TEST_BIN)
	@echo "Running tests..."
	@./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(CORE_SRC) core/respirosync_core.h
	@mkdir -p tests
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TEST_BIN) $(TEST_SRC) $(CORE_SRC)

# Clean build artifacts
clean:
	rm -f $(CORE_OBJ) $(CORE_LIB) $(TEST_BIN)

# Build with extra strict warnings (for development)
strict: CXXFLAGS += -Werror -Wshadow -Wconversion
strict: clean all

# Help target
help:
	@echo "RespiroSync Build System"
	@echo "========================"
	@echo ""
	@echo "Targets:"
	@echo "  all     - Build library and tests (default)"
	@echo "  test    - Build and run tests"
	@echo "  clean   - Remove build artifacts"
	@echo "  strict  - Build with extra strict warnings"
	@echo "  help    - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  make          # Build everything"
	@echo "  make test     # Run tests"
	@echo "  make clean    # Clean up"
