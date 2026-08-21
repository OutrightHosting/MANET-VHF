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
| `frame` | On-air frame layout, header pack/unpack, TTL, origin ID + sequence |
| `slot` | TDMA slot state machine, frame timing, pipelining rule (ADR-0002) |
| `neighbour` | Directly-heard neighbour table with link quality, ageing |
| `mpr` | Multipoint relay selection over the two-hop neighbourhood (ADR-0003) |
| `dedup` | Duplicate suppression window, passive acknowledgement |
| `node` | Composition of the above; the object the platform instantiates |

`frame` and `slot` come first — they are what [OQ-0002](../docs/open-questions.md#oq-0002) and
[OQ-0009](../docs/open-questions.md#oq-0009) are about, and nothing above them can be settled until
they are.

## What is *not* here

Codec2, the CC1200 driver, GPS/PPS handling, the audio path, and the simulated propagation model.
All platform. The core does not know whether it is running on a bench radio or inside a Python
process, and that is the point.
