# ADR-0006: One protocol implementation in C, driven by a Python simulation harness

**Status:** Accepted
**Date:** 2026-08-21
**Phase:** Pre-development

## Context

Phase 0's stated purpose is: "If the protocol fails here it will fail in hardware." That claim only
holds if the thing simulated and the thing flashed are the same thing.

The usual Phase 0 approach — model the protocol in ns-3 or Python, then reimplement it in C for the
target — breaks that guarantee at exactly the point it matters. Two implementations means two
behaviours, and the differences show up as bugs in Phase 1 that Phase 0 certified as absent. Worse,
the class of bug a slotted MAC actually suffers from — an off-by-one in slot indexing, a state
machine that mishandles a rejoin, a duplicate-suppression window that wraps wrong — is precisely the
class that a reimplementation silently perturbs.

The protocol core is also small. A TDMA state machine, neighbour table, MPR selection, duplicate
suppression and TTL handling is not a large body of code. There is no scale argument for writing it
twice.

## Decision

**One implementation of the MAC and routing logic, written in C99, in `core/`.**

The same translation units compile into:

- the Python simulation harness in `sim/`, loaded via CFFI, and
- the STM32F4 firmware in `firmware/` from Phase 1 onward.

Not a port. Not a shared spec. The same `.c` files.

The core is **sans-I/O**: a pure state machine that owns no clock, no radio, no memory allocator and
no output stream. It is driven entirely by calls, and it requests work entirely by returning
actions. Everything platform-specific — the CC1200 driver, GPS PPS, the audio path, the simulated
propagation model — lives outside it.

Python's role is the harness only: virtual clock, mobility model, propagation and collision model,
scenario definition, metrics and plotting. It is deliberately the layer where iteration is cheap and
where nothing needs to be correct on the target.

## Consequences

### Constraints the core must obey

These are not style preferences. Each one is load-bearing for "same code":

| Rule | Why |
|---|---|
| No dynamic allocation. Fixed-size tables, sized by compile-time constants | No allocator on the target; and a sim that never hits the table limits proves nothing about firmware that will |
| No floating point in the core | x86 and ARM float results differ. Integer and fixed-point arithmetic makes sim and target bit-identical, which is the entire point |
| No time source inside. Time is a parameter | The sim drives a virtual clock at arbitrary speed; the target drives a real one. Neither is the core's business |
| No randomness inside. Seeds are injected | Reproducible failures. A scenario that breaks must break identically every run |
| No I/O and no `printf`. Events go to a caller-supplied sink | The sim logs to a metrics collector; the target logs to a UART or not at all |
| Freestanding C99. No libc beyond `<stdint.h>`, `<stdbool.h>`, `<string.h>` | Portability to the target, and it keeps the core honest about its dependencies |
| Built `-Wall -Wextra -Werror -pedantic` for both targets | Divergence between the two builds should be a build failure, not a Phase 1 surprise |

### What follows

- Phase 0 results transfer to Phase 1 as evidence, not as suggestion.
- A bug found on the bench in Phase 1 can be reproduced in the simulator, because the state machine
  is identical. This is worth a great deal when debugging something with 15 ms timing.
- Regression tests written against Phase 0 scenarios keep running for the life of the project, on
  the code that ships.
- The transceiver abstraction required by [ADR-0005](0005-cc1200-stm32-bench-platform.md) — so that
  a CC1200 bench part and a possible CMX7164 production part do not leak into protocol logic — is
  the same boundary, and comes free.
- Cost: the core is less pleasant to write than Python would be, and the harness needs a CFFI build
  step. This is a real cost and it is small against the alternative.
- The core must be built as a *per-node* state object with no globals, since the simulator
  instantiates twelve of them in one process. No `static` mutable state anywhere in `core/`.

## Reversal trigger

None anticipated. If the core grows to a size where C becomes the bottleneck on protocol iteration,
that is a signal the wrong things have been put inside the boundary — the fix is to move them out to
the harness, not to abandon the shared implementation.

## Alternatives rejected

- **ns-3.** A bespoke slotted MAC with pipelined relaying means fighting ns-3's abstractions rather
  than using them, and it still leaves the protocol logic in a form that cannot be flashed.
- **Python model, C rewrite later.** The default approach, and the one that voids Phase 0's warranty.
- **Everything in Python, transpiled.** Adds a translation step with its own bug surface, in exchange
  for nothing.
- **Everything in C, including the harness.** Mobility models, scenario scripting, metrics and
  plotting are where iteration speed matters most and target-correctness matters not at all.
