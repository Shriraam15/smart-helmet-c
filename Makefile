# Smart Helmet Safety System - build file
# Usage:  make        (build)
#         make run    (build + run)
#         make clean  (remove build artifacts)

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic
LDFLAGS =

SRC_DIR = src
BUILD   = build
BIN     = $(BUILD)/smart_helmet

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD)/%.o,$(SRCS))

.PHONY: all run clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

run: all
	./$(BIN)

clean:
	rm -rf $(BUILD)
