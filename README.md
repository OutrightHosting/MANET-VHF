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
| [Addendum 01](docs/addendum-01-packet-architecture.md) | Packet architecture and gateway nodes. An architectural constraint, not a feature list. Read before writing any MAC, framing or routing code |
| [Decision log](docs/decisions/) | What has been decided and why — each with the observation that would reverse it |
| [Open questions](docs/open-questions.md) | What has not been decided, what it blocks, and when it can be answered |

## Layout

```
core/       protocol core — C99, freestanding. The code that ships.
              addr, frame built; slot next
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
make budget         slot budget across candidate frame structures (OQ-0002)
make freestanding   assert the core pulls in no libc beyond mem*
make arm            compile the core for cortex-m4; skips if the toolchain is absent
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

**[OQ-0002](docs/open-questions.md#oq-0002) — the slot budget does not close, structurally.** At
19.2 kbps a 15 ms slot yields ~264 on-air bits after a DMR-proportional guard. Sync takes 24, the
frame header 34, Codec2 3200 takes 192 — leaving **14 bits for FEC**. That is a CRC, not error
correction, on a channel with no retransmission. Five escape routes are costed in the tracker; the
strongest is 3 slots × 20 ms, which restores a 45% FEC ratio. None is picked. This is the
simulator's first job, ahead of the five questions in the brief.

[OQ-0012](docs/open-questions.md#oq-0012) — header field widths. Every bit is a bit of FEC, and
there are only 14 to spend. The header format is the one artefact that cannot change after radios
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
