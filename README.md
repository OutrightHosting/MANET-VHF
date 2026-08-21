# MANET-VHF

Multi-hop mesh voice radio for licensed UK VHF Business Radio spectrum.

Every handset is also a relay. Voice reaches its destination by hopping through other handsets in
the group — no repeater, no base station, no infrastructure, no cellular, no internet. Narrowband,
voice only, 25 kHz simplex on VHF.

**Status: pre-development.** Nothing is built. This repository currently contains the target
definition, the decisions already taken, and the questions still open.

## Start here

| | |
|---|---|
| [Engineering brief](docs/engineering-brief.md) | What this is, why it exists, and what "done" looks like. Read first. |
| [Decision log](docs/decisions/) | What has been decided and why — each with the observation that would reverse it |
| [Open questions](docs/open-questions.md) | What has not been decided, what it blocks, and when it can be answered |

## Layout

```
core/       protocol core — C99, freestanding. The code that ships.
sim/        Phase 0 simulation harness — Python, drives core/ via CFFI
firmware/   Phase 1 STM32F4 target — empty until Phase 0 passes
docs/       brief, decision log, open questions
```

The single architectural commitment worth knowing before reading anything else: **the MAC and
routing logic exists once, in C, in `core/`.** The simulator and the firmware compile the same
translation units. Phase 0's warranty — "if the protocol fails here it will fail in hardware" — only
holds if the thing simulated is the thing flashed. See
[ADR-0006](docs/decisions/0006-c-core-python-harness.md).

## Phases

| Phase | Scope | Cost | Gate |
|---|---|---|---|
| **0** | Simulate MAC and routing | None | Converges with 12 mobile nodes; cluster case relays nothing; 5-hop chain under 300 ms |
| 1 | Bench RF through attenuators | Few hundred £ | Intelligible Codec2 3200 across three hops, slot timing within guard |
| 2 | Over the air, real terrain | Ofcom Innovation and Trial licence | Two dispersed groups converse through intermediates, no user action |
| 3 | Productisation | — | RED conformity, self-assessed under Module A |

Phase 0 is where the project is. Do not buy hardware until it passes.

## Before Phase 0 starts

[OQ-0002](docs/open-questions.md#oq-0002) — the slot budget does not close as specified. At
19.2 kbps a 15 ms slot yields ~264 usable bits after a DMR-proportional guard interval; Codec2 3200
plus 47% FEC needs 282. Four ways out, each changing something currently marked decided. This is the
simulator's first job, ahead of the five questions in the brief.

[OQ-0009](docs/open-questions.md#oq-0009) — channel access is unspecified. The brief defines a frame
and a pipelining rule but never says how a node acquires the right to originate, or what happens
when two leaders press PTT in the same frame.

## Regulatory position

Designing, building and bench testing into dummy loads requires no permission. Over-the-air
transmission requires authorisation under the Wireless Telegraphy Act 2006 s.8 — an Ofcom Innovation
and Trial licence during development. Operational use requires conformity to IR 2044. Details in
[brief §7](docs/engineering-brief.md#7-regulatory-constraints).
