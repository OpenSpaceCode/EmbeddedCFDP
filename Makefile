CC ?= cc
AR ?= ar
# Build flags shared by the library, example and tests.
CFLAGS ?= -O2 -Iinclude -Wall -Wextra -std=c11
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
LIB_DIR = $(BUILD_DIR)/lib
LIB = $(LIB_DIR)/libcfdp.a

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(SRCS))

CTEST_PATH = $(BUILD_DIR)/tests/ctest
EXAMPLE_PATH = $(BUILD_DIR)/examples/example

all: lib ctest example

lib: $(LIB)

$(LIB): $(OBJS)
	mkdir -p $(dir $@)
	$(AR) rcs $@ $(OBJS)

$(OBJ_DIR)/%.o: src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

ctest: $(CTEST_PATH)

$(CTEST_PATH): tests/unit_tests.c $(LIB)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Itests tests/unit_tests.c $(LIB) -o $@

example: $(EXAMPLE_PATH)

$(EXAMPLE_PATH): examples/example.c $(LIB)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) examples/example.c $(LIB) -o $@

run: ctest
	$(CTEST_PATH)

coverage-html:
	bash tools/coverage-html.sh

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all lib ctest example run clean coverage-html
