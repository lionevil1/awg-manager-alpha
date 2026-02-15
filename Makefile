CC ?= gcc
STRIP ?= strip
CFLAGS ?= -O2 -pipe -Wall -Wextra -Wpedantic -std=c11 -D_GNU_SOURCE
LDFLAGS ?=
LDLIBS ?=

TEST_CC ?= gcc
TEST_CFLAGS ?= -O0 -g -Wall -Wextra -Wpedantic -std=c11 -D_GNU_SOURCE
TEST_LDFLAGS ?=
TEST_LDLIBS ?=

SRC_DIR := src
BUILD_DIR := build
TARGET := $(BUILD_DIR)/awg-manager-alpha
TEST_TARGET := $(BUILD_DIR)/unit-tests

SRCS := \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/config.c \
	$(SRC_DIR)/session.c \
	$(SRC_DIR)/hash.c \
	$(SRC_DIR)/router_auth.c \
	$(SRC_DIR)/server.c

OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

TEST_SRCS := \
	tests/test_main.c \
	$(SRC_DIR)/hash.c \
	$(SRC_DIR)/session.c \
	$(SRC_DIR)/config.c

.PHONY: all clean aarch64 test

all: $(TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

aarch64:
	$(MAKE) clean
	$(MAKE) CC=aarch64-linux-gnu-gcc STRIP=aarch64-linux-gnu-strip TARGET=$(TARGET)
	aarch64-linux-gnu-strip --strip-unneeded $(TARGET) || true

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_TARGET): $(TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(TEST_CC) $(TEST_CFLAGS) -I$(SRC_DIR) $(TEST_LDFLAGS) -o $@ $(TEST_SRCS) $(TEST_LDLIBS)

clean:
	rm -rf $(BUILD_DIR)
