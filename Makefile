# MANET-VHF — protocol core and Phase 0 tooling.
#
# The core is freestanding C99 and compiles unchanged for both the host (driving the
# simulator) and the STM32F4 target. See docs/decisions/0006-c-core-python-harness.md.

CC       ?= cc
BUILD    := build

CSTD     := -std=c99
WARN     := -Wall -Wextra -Werror -pedantic -Wshadow -Wconversion -Wsign-conversion
INC      := -Icore/include -Icore/src
CFLAGS   := $(CSTD) $(WARN) $(INC) $(EXTRA_CFLAGS)

CORE_SRC := $(wildcard core/src/*.c)
CORE_OBJ := $(patsubst core/src/%.c,$(BUILD)/core/%.o,$(CORE_SRC))
TEST_SRC := $(wildcard core/tests/*.c)
TEST_OBJ := $(patsubst core/tests/%.c,$(BUILD)/tests/%.o,$(TEST_SRC))

ARM_CC   := arm-none-eabi-gcc

.PHONY: all test budget freestanding arm clean

all: test

# ---------------------------------------------------------------------- tests --

test: $(BUILD)/run_tests
	@$(BUILD)/run_tests

$(BUILD)/run_tests: $(CORE_OBJ) $(TEST_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/core/%.o: core/src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/tests/%.o: core/tests/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -Icore/tests -c -o $@ $<

# --------------------------------------------------------------------- budget --

# Sweep the slot budget across candidate frame structures. See OQ-0002.
budget:
	@CC='$(CC)' ./tools/budget.sh $(BUILD)

# ---------------------------------------------------------------- freestanding --

# Enforce ADR-0006 mechanically: the core must pull in nothing from libc beyond the
# handful of mem* functions <string.h> is permitted for. Anything else — printf, malloc,
# a math routine, a float helper — appears here as an undefined symbol and fails.
#
# manet_* symbols are the core's own cross-module references and are expected.
freestanding: $(CORE_OBJ)
	@echo "checking core objects for libc dependencies"
	@undef=$$(nm -u $(CORE_OBJ) 2>/dev/null \
	          | sed 's/^ *//; s/^U //' \
	          | grep -v '^$$' \
	          | grep -v ':$$' \
	          | sed 's/^_//' \
	          | grep -v '^manet_' \
	          | grep -vE '^(memcpy|memset|memmove|memcmp)$$' \
	          | sort -u); \
	if [ -n "$$undef" ]; then \
	    echo "FAIL: core depends on:"; echo "$$undef" | sed 's/^/  /'; exit 1; \
	fi; \
	echo "ok: no libc dependencies beyond mem*"

# ------------------------------------------------------------------ arm target --

# Proves the "same code" claim in ADR-0006: the core compiles for the real target.
# Skipped with a message if the toolchain is not installed.
arm:
	@if ! command -v $(ARM_CC) >/dev/null 2>&1; then \
	    echo "skip: $(ARM_CC) not installed"; \
	    echo "      brew install --cask gcc-arm-embedded"; \
	    exit 0; \
	fi; \
	mkdir -p $(BUILD)/arm; \
	for src in $(CORE_SRC); do \
	    obj=$(BUILD)/arm/$$(basename $$src .c).o; \
	    echo "$(ARM_CC) -c $$src"; \
	    $(ARM_CC) $(CSTD) $(WARN) $(INC) \
	        -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
	        -ffreestanding -Os -c -o $$obj $$src || exit 1; \
	done; \
	echo "ok: core builds for cortex-m4"

clean:
	rm -rf $(BUILD)
