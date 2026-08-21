# `sim/` — Phase 0 simulation harness (Python)

Drives the C core in [`core/`](../core) via CFFI. Owns everything the core deliberately does not:
virtual clock, mobility, propagation, collisions, scenarios, metrics, plots.

Python because this is where iteration is cheap and where nothing has to be correct on the target.
The protocol logic under test is not Python — see
[ADR-0006](../docs/decisions/0006-c-core-python-harness.md).

## Responsibilities

| Layer | Job |
|---|---|
| Clock | Virtual time in integer microseconds, advanced by the event loop, not by wall clock |
| Mobility | Node positions over time — walking-pace group models: clustered, strung-out along a path, splinter groups, partition and rejoin |
| Propagation | Which nodes hear which, with terrain occlusion. Does not need to be a real path-loss model to answer Phase 0's questions; it needs to be *controllable* |
| Collision | Two transmissions in one slot within range of a receiver — what that receiver gets |
| Scenario | Declarative descriptions of the five Phase 0 questions, runnable as regression tests |
| Metrics | Convergence time, relay counts, end-to-end latency per hop count, beacon overhead as % of channel capacity |

## Phase 0 must answer

From [engineering brief §6](../docs/engineering-brief.md#phase-0--simulation-no-hardware-no-cost):

1. Does MPR selection converge with 12 nodes moving?
2. Does the cluster case correctly produce zero relaying?
3. Does slot pipelining resolve correctly across 3–5 hops?
4. What is beacon overhead as a percentage of channel capacity?
5. What happens when the network partitions and rejoins?

Plus, ahead of all five, [OQ-0002](../docs/open-questions.md#oq-0002) — the slot budget arithmetic
that does not currently close. There is little point converging a routing protocol onto a frame
structure that cannot carry the payload.

Gross bit rate must be a **swept parameter**, not a constant — see
[OQ-0001](../docs/open-questions.md#oq-0001).

## Success criteria

Protocol converges with 12 mobile nodes; cluster case produces zero relaying; 5-hop chain resolves
within 300 ms.
