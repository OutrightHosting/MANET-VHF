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
| **[How it works](docs/how-it-works.md)** | **One burst of speech from pressing PTT to voice coming out, and why each mechanism is there. The live reference for the numbers — read this first.** |
| [Engineering brief](docs/engineering-brief.md) | What this is, why it exists, and what "done" looks like. **Note: §4's figures are stale — see how-it-works.md** |
| [Addendum 01](docs/addendum-01-packet-architecture.md) | Packet architecture and gateway nodes. An architectural constraint, not a feature list. Read before writing any MAC, framing or routing code |
| [Feature set](docs/feature-set.md) | What the product is: a digital radio system with dispatch-grade features, where the mesh replaces the repeater as transport |
| [Decision log](docs/decisions/) | What has been decided and why — each with the observation that would reverse it |
| [Open questions](docs/open-questions.md) | What has not been decided, what it blocks, and when it can be answered |

## Layout

```
core/       protocol core — C99, freestanding. The code that ships.
              addr, frame, slot, neighbour, mpr built
sim/        Phase 0 simulation harness — Python, drives core/ via CFFI
firmware/   Phase 1 STM32F4 target — empty until Phase 0 passes
tools/      budget sweep and other Phase 0 instrumentation
docs/       brief, addendum, decision log, open questions
```

Two architectural commitments worth knowing before reading anything else.

**The MAC and routing logic exists once, in C, in `core/`.** The simulator and the firmware compile
the same translation units. Phase 0's warranty — "if the protocol fails here it will fail in
hardware" — only holds if the thing simulated is the thing flashed.
([ADR-0006](docs/decisions/0006-c-core-python-harness.md))

**This is a packet network that carries voice, not a voice system with signalling attached.** Every
transmission is an addressed, typed frame; voice is one type among several; the routing layer reads
headers and never payload. Not a request for features — a constraint on structure, so that adding
text, position reporting or a dispatch console later does not mean rewriting the MAC.
([ADR-0007](docs/decisions/0007-packet-switched-frame-architecture.md))

## Building

```bash
make test
```

```
make test           unit tests, host build
make test-3slot     the whole suite rebuilt at 3 x 20 ms, the leading OQ-0002 escape
make budget         slot budget across candidate frame structures (OQ-0002)
make freestanding   assert the core pulls in no libc beyond mem*
make arm            build for cortex-m4, check for float/libc, report flash and RAM
```

## Phases

| Phase | Scope | Cost | Gate |
|---|---|---|---|
| **0** | Simulate MAC and routing | None | Converges with 12 mobile nodes; cluster case relays nothing; 5-hop chain under 300 ms |
| 1 | Bench RF through attenuators | Few hundred £ | Intelligible Codec2 3200 across three hops, slot timing within guard |
| 2 | Over the air, real terrain | Ofcom Innovation and Trial licence | Two dispersed groups converse through intermediates, no user action |
| 3 | Productisation | — | RED conformity, self-assessed under Module A |

Phase 0 is where the project is. Do not buy hardware until it passes.

## Before Phase 0 starts

**[OQ-0002](docs/open-questions.md#oq-0002) — the slot budget is tight, and every figure in it
is an assumption.** At 19.2 kbps a 40 ms slot yields 704 on-air bits after a DMR-proportional
guard. Sync takes 56, the frame header 42, Codec2 3200 takes 512 — leaving **94 bits for FEC,
16%**, against DMR's 47%. Verify with `make budget`.

That is a great deal better than the 2% this section reported when the frame was 60 ms, and the
whole improvement came from re-deriving the acquisition preamble
([docs/preamble-budget.md](docs/preamble-budget.md)) rather than from anything clever in the
protocol. It is still not enough, and the two levers that close it — gross bit rate
([OQ-0001](docs/open-questions.md#oq-0001)) and vocoder rate — are both bench measurements.

[OQ-0012](docs/open-questions.md#oq-0012) — header field widths. Every bit is a bit of FEC, and
there are 94 to spend. The header format is the one artefact that cannot change after radios
ship.

[OQ-0013](docs/open-questions.md#oq-0013) — spatial reuse distance. Under pipelining the originator
and the node N hops away transmit in the same slot simultaneously, so slot count *is* reuse
distance. This decides whether the 3-slot escape from OQ-0002 is safe, and it is the real cost of
that route — not hop latency, which the brief identifies as the trade but which passes with large
margin either way.

[OQ-0009](docs/open-questions.md#oq-0009) — channel access is unspecified. The brief defines a frame
and a pipelining rule but never says how a node acquires the right to originate, or what happens
when two leaders press PTT in the same frame. Addendum 01 widens this: four priority classes now
contend for the same slots, with a stated policy and no mechanism.

## Regulatory position

Designing, building and bench testing into dummy loads requires no permission. Over-the-air
transmission requires authorisation under the Wireless Telegraphy Act 2006 s.8 — an Ofcom Innovation
and Trial licence during development. Operational use requires conformity to IR 2044. Details in
[brief §7](docs/engineering-brief.md#7-regulatory-constraints).
