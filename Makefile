CC        := gcc
BASEFLAGS := -std=c11 -D_GNU_SOURCE -Wall -Wextra -g -pthread -Iinclude
CFLAGS    := $(BASEFLAGS) -MMD -MP
LDFLAGS   := -pthread

SRC_DIR   := src
BUILD     := build
TARGET    := proxy

SRCS      := $(wildcard $(SRC_DIR)/*.c)
OBJS      := $(SRCS:$(SRC_DIR)/%.c=$(BUILD)/%.o)
LIB_SRCS  := $(filter-out $(SRC_DIR)/main.c,$(SRCS))

.PHONY: all run clean test test_cache test_http dirs

all: dirs $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Build complete. Start the proxy with: ./$(TARGET)"

$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

dirs:
	mkdir -p logs

run: all
	./$(TARGET)
test_cache: | $(BUILD)
	$(CC) $(BASEFLAGS) tests/test_cache.c $(SRC_DIR)/cache.c -o $(BUILD)/test_cache $(LDFLAGS)
	./$(BUILD)/test_cache
test_http: | $(BUILD)
	$(CC) $(BASEFLAGS) tests/test_http_handler.c $(LIB_SRCS) -o $(BUILD)/test_http $(LDFLAGS)
	./$(BUILD)/test_http

test: test_cache

clean:
	rm -rf $(BUILD) $(TARGET)
-include $(OBJS:.o=.d)