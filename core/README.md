# `core/` — protocol core (C99, freestanding)

The MAC and routing state machine. **This is the code that ships.** The simulator in `sim/` and the
Phase 1 firmware in `firmware/` compile these same translation units — see
[ADR-0006](../docs/decisions/0006-c-core-python-harness.md).

## Rules

Non-negotiable, because "same code" depends on every one of them:

- **No dynamic allocation.** Fixed-size tables, compile-time constants.
- **No floating point.** Integer and fixed-point only, so x86 and ARM produce identical results.
- **No time source.** Time arrives as a parameter, in integer microseconds.
- **No randomness.** Seeds are injected; failures reproduce exactly.
- **No I/O, no `printf`.** Events go to a caller-supplied sink.
- **No mutable `static` state, no globals.** The simulator runs twelve nodes in one process; every
  node is a separate state object.
- **Freestanding C99.** `<stdint.h>`, `<stdbool.h>`, `<string.h>`. Nothing else.
- **Builds clean** under `-Wall -Wextra -Werror -pedantic` for both host and `arm-none-eabi`.

And from [ADR-0007](../docs/decisions/0007-packet-switched-frame-architecture.md):

- **The routing layer reads header fields only — never payload.** Priority is a header field, so
  pre-emption does not violate this.
- **No voice special-casing anywhere below the dispatcher.** Voice is one frame type. If a `switch`
  on frame type appears outside `dispatch`, something is in the wrong place.
- **No assumption that a node is a handheld.** Gateways run the identical MAC and routing; the
  routing layer must not be able to tell them apart.

Anything that wants to break one of these rules belongs in the platform layer, not here.

## Shape

Sans-I/O. The core is driven by calls and requests work by returning actions. It never reaches out.

```
include/manet/    public API — one header per module, no internal types exposed
src/              implementation
```

Intended modules, in dependency order:

| Module | Responsibility |
|---|---|
| `addr` | ✅ Address space, range predicates (individual / gateway / group / broadcast / reserved) |
| `frame` | ✅ On-air header pack/unpack — source, destination, type, sequence, TTL, priority |
| `slot` | ✅ TDMA slot state machine, frame timing, pipelining rule (ADR-0002) |
| `queue` | Priority queue, four classes, pre-emption policy (Addendum 01 §5) |
| `neighbour` | Directly-heard neighbour table with link quality, ageing |
| `mpr` | Multipoint relay selection over the two-hop neighbourhood (ADR-0003) |
| `dedup` | Duplicate suppression window, passive acknowledgement |
| `dispatch` | Receive path — switch on frame type. The **only** place that knows what a payload is |
| `node` | Composition of the above; the object the platform instantiates |

`addr`, `frame` and `slot` are built (✅). `neighbour` and `mpr` are next. The header format is the one artefact here that
cannot be changed after radios ship — see [OQ-0012](../docs/open-questions.md#oq-0012).

`config.h` holds the compile-time parameters and computes the slot budget from them, enforced by
static assertion: a configuration that cannot carry its own sync, header and voice payload fails to
build. This is what makes [OQ-0002](../docs/open-questions.md#oq-0002) something you sweep rather
than something you argue about.

## Building

```
make test           unit tests, host build
make test-3slot     the whole suite rebuilt at 3 x 20 ms, the leading OQ-0002 escape
make budget         slot budget across candidate frame structures (OQ-0002)
make freestanding   assert the core pulls in no libc beyond mem*
make arm            build for cortex-m4, check for float/libc, report flash and RAM
```

Built `-Wall -Wextra -Werror -pedantic -Wshadow -Wconversion -Wsign-conversion`.

`make freestanding` and `make arm` are how the ADR-0006 rules stop being aspirational: both read the
undefined symbols out of the core objects and fail if anything reaches for libc. A stray `printf` or
`malloc` shows up there.

`make arm` carries more weight than the host check. An accidental `double` links silently against
x86 hardware floating point, but on cortex-m4 it pulls in `__aeabi_dadd` and friends — so the ARM
build is what actually enforces the no-floating-point rule, and with it the bit-identical behaviour
between simulator and firmware that the whole approach depends on.

libgcc *integer* helpers are permitted and `__aeabi_uldivmod` is expected: cortex-m4 has no 64-bit
divide instruction. Floating-point helpers are a hard failure.

Current cost on target, with `addr`, `frame` and `slot` built: **1269 bytes of flash, 0 data,
0 bss.** The two zeroes are the "no globals, no mutable statics" rule confirmed by the linker rather
than by inspection.

A note on `_Static_assert`: it is C11, and this core is C99. `config.h` uses it when the translation
unit is compiled as C11 or later — the messages carry OQ references and are worth having — and falls
back to the negative-array-size idiom otherwise.

## What is *not* here

Codec2, the CC1200 driver, GPS/PPS handling, the audio path, the gateway's wired interface, and the
simulated propagation model. All platform. The core does not know whether it is running on a bench
radio, inside a gateway, or inside a Python process, and that is the point.
