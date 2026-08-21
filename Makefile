# MANET-VHF — protocol core and Phase 0 tooling.
#
# The core is freestanding C99 and compiles unchanged for both the host (driving the
# simulator) and the STM32F4 target. See docs/decisions/0006-c-core-python-harness.md.

CC       ?= cc
BUILD    := build

CSTD     := -std=c99
WARN     := -Wall -Wextra -Werror -pedantic -Wshadow -Wconversion -Wsign-conversion
INC      := -Icore/include -Icore/src

# The stack protector calls __stack_chk_fail, which is libc runtime the bare-metal
# target does not have — the arm build would fail to link. Standard for freestanding
# code, and `make freestanding` is what caught it when slot.c grew a PDU buffer.
FREE     := -fno-stack-protector

DEPFLAGS := -MMD -MP
CFLAGS   := $(CSTD) $(WARN) $(INC) $(FREE) $(DEPFLAGS) $(EXTRA_CFLAGS)

CORE_SRC := $(wildcard core/src/*.c)
CORE_OBJ := $(patsubst core/src/%.c,$(BUILD)/core/%.o,$(CORE_SRC))
TEST_SRC := $(wildcard core/tests/*.c)
TEST_OBJ := $(patsubst core/tests/%.c,$(BUILD)/tests/%.o,$(TEST_SRC))
DEPS     := $(CORE_OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(ARM_OBJ:.o=.d)

ARM_CC   := arm-none-eabi-gcc

.PHONY: FORCE all test test-3slot budget freestanding arm arm-build arm-check arm-size trace sim sim-lib reuse clean

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

# Rebuild and run the whole suite against the leading escape route from OQ-0002.
# Proves the core is genuinely parameterised rather than quietly assuming four slots —
# which matters, because the simulator sweeps it.
test-3slot:
	@$(MAKE) -s clean
	@$(MAKE) -s test EXTRA_CFLAGS="-DMANET_SLOTS_PER_FRAME=3L" BUILD=$(BUILD)
	@$(MAKE) -s clean

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

ARM_OBJ   := $(patsubst core/src/%.c,$(BUILD)/arm/%.o,$(CORE_SRC))
ARM_FLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -ffreestanding -Os

# Proves the "same code" claim in ADR-0006: the core compiles for the real target, and
# depends on nothing the target cannot provide.
#
# The dependency check matters MORE here than on the host. A stray double or float in
# the core links silently against x86 hardware FP, but on ARM it pulls in __aeabi_dadd
# and friends — so this is the build that actually enforces the no-floating-point rule,
# and with it the bit-identical behaviour the simulator's warranty rests on.
arm:
	@if ! command -v $(ARM_CC) >/dev/null 2>&1; then \
	    echo "skip: $(ARM_CC) not installed"; \
	    echo "      brew install --cask gcc-arm-embedded"; \
	    exit 0; \
	fi; \
	$(MAKE) -s arm-build arm-check arm-size

arm-build: $(ARM_OBJ)

$(BUILD)/arm/%.o: core/src/%.c
	@mkdir -p $(@D)
	$(ARM_CC) $(CSTD) $(WARN) $(INC) $(FREE) $(DEPFLAGS) $(ARM_FLAGS) -c -o $@ $<

# libgcc integer helpers are permitted: cortex-m4 has no 64-bit divide instruction, so
# uint64 arithmetic compiles to a call. Every embedded ARM build links libgcc.
ARM_ALLOWED := memcpy|memset|memmove|memcmp\
|__aeabi_memcpy|__aeabi_memcpy4|__aeabi_memcpy8|__aeabi_memset|__aeabi_memclr|__aeabi_memclr4|__aeabi_memclr8\
|__aeabi_uldivmod|__aeabi_ldivmod|__aeabi_uidiv|__aeabi_idiv|__aeabi_uidivmod|__aeabi_idivmod\
|__aeabi_lmul|__aeabi_llsl|__aeabi_llsr|__aeabi_lasr\
|__udivdi3|__umoddi3|__divdi3|__moddi3|__muldi3

arm-check: $(ARM_OBJ)
	@syms=$$(arm-none-eabi-nm -u $(ARM_OBJ) 2>/dev/null \
	         | sed 's/^ *//; s/^U //' \
	         | grep -v '^$$' | grep -v ':$$' | grep -v '^manet_' | sort -u); \
	float=$$(printf '%s\n' "$$syms" | grep -E '^(__aeabi_[df]|__aeabi_[a-z]+2[df]|__[a-z]+[sd]f[0-9])' || true); \
	if [ -n "$$float" ]; then \
	    echo "FAIL: floating point has crept into the core — ADR-0006 forbids it,"; \
	    echo "      because x86 and ARM float results differ and the simulator's"; \
	    echo "      warranty depends on them being bit-identical:"; \
	    printf '%s\n' "$$float" | sed 's/^/  /'; exit 1; \
	fi; \
	other=$$(printf '%s\n' "$$syms" | grep -v '^$$' | grep -vE '^($(ARM_ALLOWED))$$' || true); \
	if [ -n "$$other" ]; then \
	    echo "FAIL: unexpected target runtime dependency:"; \
	    printf '%s\n' "$$other" | sed 's/^/  /'; exit 1; \
	fi; \
	echo "ok: no floating point, no libc"; \
	helpers=$$(printf '%s\n' "$$syms" | grep -E '^__' || true); \
	if [ -n "$$helpers" ]; then \
	    echo "    libgcc integer helpers linked (expected):"; \
	    printf '%s\n' "$$helpers" | sed 's/^/      /'; \
	fi

arm-size: $(ARM_OBJ)
	@echo "flash and RAM cost of the protocol core:"
	@arm-none-eabi-size -t $(ARM_OBJ) | sed 's/^/  /'

clean:
	rm -rf $(BUILD)

# Header dependencies, generated by the compiler. Without these a change to config.h
# leaves stale objects behind and the suite passes against code that no longer exists.
-include $(DEPS)

# --------------------------------------------------------------------- sim --

UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
SHLIB := $(BUILD)/libmanetcore.dylib
else
SHLIB := $(BUILD)/libmanetcore.so
endif

# The harness drives this exact library. Same translation units as the tests and the
# cortex-m4 build — ADR-0006. -fvisibility=hidden means only the bridge's own entry
# points are exported, so the core's internals stay internal.
sim-lib: $(SHLIB)

# Rebuild when the FLAGS change, not just the sources. Without this a sweep over
# EXTRA_CFLAGS silently measures whatever library happened to be built last.
$(BUILD)/.cflags: FORCE
	@mkdir -p $(@D)
	@echo '$(CFLAGS)' | cmp -s - $@ || echo '$(CFLAGS)' > $@
FORCE:

$(SHLIB): $(CORE_SRC) sim/bridge.c $(wildcard core/include/manet/*.h) $(BUILD)/.cflags
	@mkdir -p $(@D)
	$(CC) $(CSTD) $(WARN) $(INC) $(FREE) $(EXTRA_CFLAGS) \
	    -fPIC -shared -fvisibility=hidden -O2 -o $@ $(CORE_SRC) sim/bridge.c

# Phase 0 experiments. Python 3, standard library only — no venv, no pip.
sim: $(SHLIB)
	@python3 sim/run.py $(ARGS)

# ------------------------------------------------------------------- trace --

# Drive the real scheduler over an idealised chain and emit what happened, as JSON.
# Shows the pipelining rule working and makes spatial reuse (OQ-0013) visible.
trace: $(BUILD)/trace
	@$(BUILD)/trace

$(BUILD)/trace: tools/trace.c $(CORE_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CSTD) $(WARN) $(INC) $(FREE) $(EXTRA_CFLAGS) -o $@ tools/trace.c $(CORE_OBJ)

# Regenerate the OQ-0013 reuse table. It drifted once because nothing could reproduce it.
reuse: $(SHLIB)
	@python3 -c "import sys;sys.path.insert(0,'.');\
from sim.manet.core import CONFIG;from sim.scenarios import reuse;\
print(CONFIG);\
[print('  %-16s collisions %4d  end-to-end %5.1f%%' % (r['env'],r['collisions'],r['delivery'][max(r['delivery'])]*100)) for r in (reuse.run(env_name=e) for e in ('woodland','open'))]"
