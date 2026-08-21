# ADR-0002: 4-slot TDMA with slot pipelining, not store-and-forward

**Status:** Accepted
**Date:** 2026-08-21 (recorded; decision predates the repository)
**Phase:** Pre-development
**Amended by:** [ADR-0007](0007-packet-switched-frame-architecture.md) — the payload arithmetic in
*Consequences* below predates the frame header, and understates the deficit. The itemised budget is
in ADR-0007; the pipelining decision itself is unchanged.

## Context

A relayed voice system's usability is decided by per-hop latency. In a dispersed group the path
may be five hops; the ultimate success criterion is that neither talker knows or cares how many
radios carried the audio. That means the total mouth-to-ear delay must stay conversational.

The naive design — a node receives a frame, then relays it in the *next* frame — costs a full
frame time per hop. At a 60 ms frame that is 60 ms/hop: 300 ms at five hops before codec, jitter
buffer and audio path are counted, and unusable beyond that.

## Decision

TDMA, 60 ms frame, 4 slots of 15 ms, with **slot pipelining**: a relaying node receives in slot *n*
and retransmits in slot *n+1* of the same frame.

A transmission therefore advances three hops within one frame, wrapping into the next frame's
slot 0 for the fourth. Average cost is ~20 ms/hop, giving 15 hops in 300 ms.

Store-and-forward is explicitly rejected. It is simpler, and it does not meet the requirement.

Synchronisation is GPS-disciplined initially. Network-derived sync is a later refinement — it works
indoors and under canopy, and it is considerably harder. See [OQ-0003](../open-questions.md).

## Consequences

- The MAC is the hard part of this project, and slot timing is the hard part of the MAC. It must be
  right in simulation ([ADR-0006](0006-c-core-python-harness.md)) before any hardware is bought.
- A relay node must complete RX → decode-enough-to-decide → TX turnaround inside one guard interval.
  On a half-duplex transceiver that turnaround is a real, measurable number and it is the first
  thing Phase 1 must characterise on the CC1200.
- Guard time is not free and cannot be trimmed to zero. EN 300 113 tests adjacent channel power
  during burst transitions; a fast, dirty slot switch fails it. Transmitter attack and release
  times must be designed in from the start, which sets a floor on guard.
- **The slot budget as specified does not close.** Recorded here rather than buried, because it is
  the single most consequential open item in the project:

  | Quantity | Value |
  |---|---|
  | Gross rate | 19.2 kbps (assumed, unverified — [OQ-0001](../open-questions.md)) |
  | Slot duration | 15 ms |
  | Raw bits per slot | 288 |
  | Guard at DMR-equivalent proportion (8.3%, i.e. 2.5 ms in 30 ms) | −24 bits |
  | **Usable payload per slot** | **~264 bits** |
  | Codec2 3200 per 60 ms frame | 192 bits |
  | + 47% FEC and framing | 282 bits |
  | **Deficit** | **~18 bits (~7%)** |

  Four ways out, all of which change something already decided: raise the gross rate to ~20.5 kbps;
  cut FEC+framing overhead from 47% to ~37%; go to 3 slots of 20 ms and accept 2 hops/frame instead
  of 3; or pack voice over a 120 ms superframe so one codec payload spans two slot opportunities.
  Resolving this is [OQ-0002](../open-questions.md) and it is Phase 0's first job.

- Where beacons and topology-control traffic live in the slot structure is **not yet specified**.
  They contend with voice for the same four slots. See [OQ-0004](../open-questions.md).

## Reversal trigger

Phase 0 shows that pipelined relaying cannot be made collision-free at 12 nodes with realistic
mobility, *or* Phase 1 measures a CC1200 RX→TX turnaround that will not fit any guard interval
compatible with EN 300 113 burst transition limits.

Note that the *slot count* moving from 4 does not reverse this ADR — pipelining is the decision,
4 slots is a parameter of it. Slot count is [OQ-0005](../open-questions.md).

## Alternatives rejected

- **Store-and-forward (relay in frame n+1).** 60 ms/hop. Unusable beyond five hops.
- **CSMA/CA.** No bounded latency, and hidden-node collisions are the normal case in a dispersed
  mesh, not the exception. Voice needs a deterministic slot.
- **DMR's 2-slot structure.** Only 1 pipelined hop per 60 ms. IWAVE's comparable product went to
  6 slots for exactly this reason.
