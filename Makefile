CC ?= cc
AR ?= ar

# Optimisation and instrumentation only. tools/coverage-html.sh overrides this
# to add gcov instrumentation, so the standard, include paths and warning set
# below stay identical between a normal build and a coverage build.
OPT ?= -O2

WARNINGS = -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-align -Wcast-qual \
           -Wpointer-arith -Wformat=2 -Wmissing-prototypes -Wstrict-prototypes \
           -Wredundant-decls -Wundef

# Build flags shared by the library, example and tests.
CFLAGS = $(OPT) -std=c99 -Iinclude $(WARNINGS)

BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
LIB_DIR = $(BUILD_DIR)/lib
LIB = $(LIB_DIR)/libcfdp.a

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(SRCS))

# Test entry point plus one test file per source module.
TEST_SRCS = $(wildcard tests/*.c)
TEST_HDRS = $(wildcard tests/*.h)
TEST_OBJS = $(patsubst tests/%.c,$(OBJ_DIR)/tests/%.o,$(TEST_SRCS))

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

# Test objects live under $(OBJ_DIR) so gcov's .gcno/.gcda files stay inside
# $(BUILD_DIR) instead of being dropped in the repository root.
$(OBJ_DIR)/tests/%.o: tests/%.c $(TEST_HDRS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Itests -c $< -o $@

$(CTEST_PATH): $(TEST_OBJS) $(LIB)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(TEST_OBJS) $(LIB) -o $@

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
