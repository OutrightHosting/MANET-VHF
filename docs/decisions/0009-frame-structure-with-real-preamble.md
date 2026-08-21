# ADR-0009: Frame structure re-derived with a measured preamble

**Status:** Accepted
**Date:** 2026-08-21
**Phase:** Phase 0
**Supersedes:** [ADR-0008](0008-four-slots.md)
**Source:** [NBWF comparison](../nbwf-comparison.md), FFI-rapport 2009/01894 §4.3

## Context

[ADR-0008](0008-four-slots.md) fixed four slots of 15 ms from two arguments: spatial reuse
requires at least four, and the voice payload permits at most four. Both used
`MANET_SYNC_BITS = 24` — a guess, with nothing behind it.

The only published figure for the same 25 kHz channel is NBWF's, and it is **8 ms of
acquisition and signalling preamble per burst** (sync 1.5 ms, Start-Of-Message 2.1 ms, Par
1.6 ms, transition 0.1 ms). At 19.2 kbps that is **154 bits**, six times our assumption.

The spatial-reuse argument is unaffected — it is about geometry, not bits. The payload
argument inverts: a 15 ms slot yields 110 bits of data, and the header and voice alone need
234. **The current frame cannot carry a voice payload at all.**

## Decision

**120 ms frame, four slots of 30 ms, voice payload spanning two slots.**

| | Value |
|---|---|
| Frame | 120 ms (six Codec2 3200 frames, 384 bits) |
| Slots | 4 × 30 ms |
| Preamble | 154 bits (8 ms) per burst |
| Data per slot | 375 bits |
| Header + half the voice payload | 234 bits |
| **Left for FEC** | **141 bits — 60%** |
| Per-hop latency | 60 ms (two slots) |
| 5-hop network latency | **300 ms** |

Four slots is retained, on the argument that survives: spatial reuse needs Δ = N−2 ≥ 2, and
Li et al.'s measured hardware figure is Δ = 1.2. Three slots still fails.

The 30 ms slot is what amortises the preamble. NBWF reached 22.5 ms slots by the same
reasoning and says so: *"one transmission burst will have to comprise several MELPe frames.
The more MELPe frames packed in a burst, the higher efficiency will be obtained."*

Voice spanning two slots is NBWF's arrangement too — MELPe 2400 needs two of their slots at
20 kbps. It costs per-hop latency, because a relay must receive both slots before forwarding,
and that is the price of paying the preamble once per two slots rather than once per one.

## Consequences

- **FEC goes from 6 bits to 141 — 60%, above DMR's 47%.** [OQ-0002](../open-questions.md#oq-0002),
  the blocking item for the whole of Phase 0, is closed by lengthening the slot rather than by
  any of the five escape routes considered. None of them was the answer; the frame was.
- **Per-hop latency triples**, 15 ms to 60 ms. Five hops is 300 ms of network latency, exactly
  the brief's stated criterion, so a five-hop chain still passes — but with no margin, where
  it previously had 4×.
- **Mouth-to-ear tightens badly.** At 120 ms packetisation plus ~60 ms jitter and codec, a
  300 ms mouth-to-ear budget (3GPP TS 22.179) allows **two hops**. A 500 ms budget allows
  five. NBWF faces the same wall: its own design reaches ~300 ms *before any relaying*, against
  a 250 ms requirement, and it cites 500 ms as the alternative.
  **Which budget applies is now a product decision, not an engineering one**, and it sets the
  usable diameter. See [OQ-0022](../open-questions.md#oq-0022).
- Packetisation doubles from 60 ms to 120 ms, which is felt at every hop count including zero.
- The slot count argument is now single-sourced. Spatial reuse still demands ≥4; the payload
  no longer constrains from above, since longer slots only help. Five or six slots become
  arguable if per-hop latency matters more than FEC — 120 ms / 5 × 24 ms gives 15% FEC and
  48 ms per hop.

## Reversal trigger

**A measured CC1200 preamble materially below 8 ms.** NBWF's figure is for NBWF's physical
layer, not ours; a simpler mode may need far less, and every number here scales with it. This
is now the most valuable measurement in Phase 1, ahead of the gross bit rate — the bit rate
tunes the budget, the preamble decides the frame.

At a preamble near zero the old 60 ms / 4 × 15 ms structure returns and with it 15 ms per hop.
At 8 ms it is unusable. The truth is somewhere between and nobody has measured it.

## Alternatives rejected

- **60 ms / 4 × 15 ms with voice over three slots.** Fits, at 4% FEC. That is a CRC.
- **90 ms / 4 × 22.5 ms, voice over two.** 30% FEC, 45 ms per hop, seven hops at a 500 ms
  budget. The better choice if reach matters more than robustness; kept as the alternate.
- **150 ms and 202.5 ms frames.** More FEC still, and packetisation alone eats the latency
  budget before a single hop.
- **Three slots.** Still fails spatial reuse, and this ADR changes nothing about that.
