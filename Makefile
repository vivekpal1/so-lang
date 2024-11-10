# Makefile for So Lang Programming Language
# Build system for the So Lang compiler

CC = gcc
CFLAGS = -Wall -Wextra -O3 -std=c99 -march=native -flto
DEBUG_FLAGS = -Wall -Wextra -g -std=c99 -DDEBUG
BUILDDIR = build
BINDIR = bin
TESTDIR = examples

# Source files (assuming they're in the current directory)
SOURCES = so_lang.c
HEADERS = so_lang.h
TARGET = $(BINDIR)/solang

all: $(TARGET)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

$(TARGET): $(SOURCES) $(HEADERS) | $(BINDIR)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)
	@echo "✓ So Lang compiler built successfully!"
	@echo "  Optimizations: -O3 -march=native -flto"

debug: $(SOURCES) $(HEADERS) | $(BINDIR)
	$(CC) $(DEBUG_FLAGS) $(SOURCES) -o $(BINDIR)/solang-debug
	@echo "✓ Debug build complete!"

# Run tests
test: $(TARGET) create-examples
	@echo "Running So Lang tests..."
	@for file in $(TESTDIR)/*.so; do \
		echo "Testing $file..."; \
		$(TARGET) $file && echo "✓ Test passed"; \
	done

# Create example programs  
create-examples: | $(TESTDIR)
	@echo 'let x = 42' > $(TESTDIR)/simple.so
	@echo 'print(x)' >> $(TESTDIR)/simple.so
	@echo '' >> $(TESTDIR)/simple.so
	@echo 'let name = "So Lang"' > $(TESTDIR)/hello.so
	@echo 'print(name)' >> $(TESTDIR)/hello.so
	@echo '' >> $(TESTDIR)/hello.so
	@echo 'let a = 10' > $(TESTDIR)/math.so
	@echo 'let b = 20' >> $(TESTDIR)/math.so
	@echo 'let sum = a + b' >> $(TESTDIR)/math.so
	@echo 'print(sum)' >> $(TESTDIR)/math.so
	@echo "✓ Example programs created in $(TESTDIR)/"

$(TESTDIR):
	mkdir -p $(TESTDIR)

# Install (copy to system path)
install: $(TARGET)
	@if [ -w /usr/local/bin ]; then \
		cp $(TARGET) /usr/local/bin/; \
		echo "✓ So Lang installed to /usr/local/bin/solang"; \
	else \
		echo "⚠ Need sudo permissions to install to /usr/local/bin"; \
		echo "  Run: sudo make install"; \
	fi

clean:
	rm -rf $(BUILDDIR) $(BINDIR) output.c output.rs
	@echo "✓ Cleaned build artifacts"

distclean: clean
	rm -rf $(TESTDIR)
	@echo "✓ Cleaned everything"

# Performance benchmark
benchmark: $(TARGET) create-examples
	@echo "Benchmarking So Lang compiler performance..."
	@time $(TARGET) $(TESTDIR)/math.so
	@echo "Compilation complete!"

# Check for memory leaks (requires valgrind)
memcheck: debug create-examples
	@if command -v valgrind >/dev/null 2>&1; then \
		echo "Running memory leak check..."; \
		valgrind --leak-check=full --show-leak-kinds=all $(BINDIR)/solang-debug $(TESTDIR)/simple.so; \
	else \
		echo "Valgrind not found. Install it to run memory checks."; \
	fi

format:
	@if command -v clang-format >/dev/null 2>&1; then \
		clang-format -i $(SOURCES) $(HEADERS); \
		echo "✓ Code formatted"; \
	else \
		echo "clang-format not found. Install it to format code."; \
	fi

info:
	@echo "So Lang Compiler Build Information:"
	@echo "  Compiler: $(CC)"
	@echo "  Release flags: $(CFLAGS)"
	@echo "  Debug flags: $(DEBUG_FLAGS)"
	@echo "  Target: $(TARGET)"
	@echo "  Source: $(SOURCES)"
	@echo ""
	@echo "Available targets:"
	@echo "  all       - Build release version (default)"
	@echo "  debug     - Build debug version"
	@echo "  test      - Run tests"
	@echo "  examples  - Create example programs"
	@echo "  install   - Install to system"
	@echo "  clean     - Remove build artifacts"
	@echo "  benchmark - Performance test"
	@echo "  memcheck  - Memory leak check"
	@echo "  format    - Format source code"

help: info

.PHONY: all debug test examples install clean distclean benchmark memcheck format info help