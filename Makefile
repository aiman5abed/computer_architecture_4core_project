# =============================================================================
# Makefile for Multi-Core MESI Simulator
# =============================================================================
# For use with GCC (MinGW on Windows, or native Linux/Mac)
# =============================================================================

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -I include
LDFLAGS = 

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

# Source files
SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/pipeline.c $(SRC_DIR)/cache.c $(SRC_DIR)/bus.c
OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/pipeline.o $(BUILD_DIR)/cache.o $(BUILD_DIR)/bus.o
TARGET = sim

# Default target
all: $(BUILD_DIR) $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# Compile
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(INC_DIR)/sim.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean
clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(TARGET).exe

# Run simple test
test: $(TARGET)
	cp tests/simple/*.txt .
	./$(TARGET)
	@echo "=== Results ==="
	@cat stats0.txt

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: clean all

.PHONY: all clean test debug
