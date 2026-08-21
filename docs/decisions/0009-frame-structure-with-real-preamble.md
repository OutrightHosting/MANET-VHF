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

**200 ms frame, four slots of 50 ms, voice payload in ONE slot.**

> **Corrected before implementation.** The first version of this ADR specified a 120 ms
> frame with voice spanning two slots, claiming 60% FEC. That figure was bought by breaking
> spatial reuse. A payload occupying M slots per hop wraps into the originator's next burst
> after N/M hops, so **reuse distance is N/M** — at M=2 it falls to two hops, which is the
> hidden-terminal case that forced four slots in the first place. Voice must occupy one slot.

| | Value |
|---|---|
| Frame | 200 ms (Codec2 3200 → 640 bits) |
| Slots | 4 × 50 ms |
| Preamble | 154 bits (8 ms) per burst |
| Data per slot | 880 bits on air |
| Header + voice | 682 bits |
| **Left for FEC at 19.2 kbps** | **44 bits — 6%** |
| Per-hop latency | 50 ms |
| Usable depth at 500 ms mouth-to-ear | **4 hops** |

**The budget does not close at 19.2 kbps.** With voice in one slot, as spatial reuse
requires, the arithmetic is unforgiving:

| Gross rate | FEC |
|---|---|
| 19.2 kbps (assumed) | 6% |
| 22.4 kbps | 28% |
| **25.6 kbps** | **49%** — exceeds DMR |

And it cannot be fixed by lengthening the frame. Under N ≥ 4M, available capacity grows as
4.40·(F/M) − 154 while voice grows as 3.2·(F/M), so the FEC ratio converges to about 37%
and **47% is unreachable at any frame length** with Codec2 3200 at 19.2 kbps. 25% would need
a 516 ms frame — longer than the whole latency budget.

Codec2 2400 reaches 25% at a 147 ms frame and 47% at 247 ms. So the two escapes are the two
the brief always listed as open: **a measured bit rate near 25 kbps, or a lower vocoder rate.**

Four slots is retainedFour slots is retained, on the argument that survives: spatial reuse needs Δ = N−2 ≥ 2, and
Li et al.'s measured hardware figure is Δ = 1.2. Three slots still fails.

The 30 ms slot is what amortises the preamble. NBWF reached 22.5 ms slots by the same
reasoning and says so: *"one transmission burst will have to comprise several MELPe frames.
The more MELPe frames packed in a burst, the higher efficiency will be obtained."*

Voice spanning two slots is NBWF's arrangement too — MELPe 2400 needs two of their slots at
20 kbps. It costs per-hop latency, because a relay must receive both slots before forwarding,
and that is the price of paying the preamble once per two slots rather than once per one.

## Consequences

- **[OQ-0002](../open-questions.md#oq-0002) does not close.** FEC goes from 6 bits to 44 —
  still 6%. Lengthening the slot recovers what the preamble costs and no more. The blocking
  item survives, and the escape routes are the ones it always had.
- **Usable depth halves, from about 8 hops to 4.** This is the largest practical consequence.
  At 50 ms per hop and 500 ms mouth-to-ear the network is four hops deep, which in woodland
  is roughly 1.9 km of dispersal. A group strung out over 3 km is beyond reach.
- **Per-hop latency more than triples**, 15 ms to 50 ms.
- **The relay stagger had to be removed.** Ranking candidates over three slots cost 150 ms per
  hop at 50 ms slots against 50 ms without, and the budget cannot pay it. Coverage-based
  suppression now does what the stagger was introduced for.
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
