# ADR-0008: Four slots per frame

**Status:** Accepted
**Date:** 2026-08-21
**Phase:** Phase 0
**Amends:** [ADR-0002](0002-tdma-slot-pipelining.md) — confirms its slot count on evidence it did not have
**Closes:** [OQ-0005](../open-questions.md#oq-0005), [OQ-0013](../open-questions.md#oq-0013)

## Context

[ADR-0002](0002-tdma-slot-pipelining.md) specified four slots of 15 ms, derived from
Codec2 3200 plus FEC at an assumed 19.2 kbps. That derivation did not close — the header
alone leaves 6 bits for error correction — and three slots of 20 ms looked like the
obvious escape, restoring a 40% FEC ratio.

Phase 0 simulation settled it, and not in favour of three.

## Decision

**Four slots of 15 ms.** Not as a preference: it is the only value that satisfies both
constraints, and it is pinned from both sides at once.

| Slots | On-air bits | Left for FEC | Reuse C/I | |
|---|---|---|---|---|
| 2 | 528 | 270 (115%) | 0.0 dB | reuse collides |
| 3 | 352 | 94 (40%) | 9.6 dB | reuse collides |
| **4** | **264** | **6 (2%)** | **15.3 dB** | **only survivor** |
| 5 | 211 | −47 | 19.3 dB | voice does not fit |
| 6 | 176 | −82 | 22.4 dB | voice does not fit |

> **Numbers corrected 2026-08-21 after adversarial audit.** The delivery figures originally
> cited here (99.8%, zero collisions) were an artifact of a beacon-scheduling defect that
> kept the control plane off the air; see [OQ-0013](../open-questions.md#oq-0013). The
> decision is unaffected — voice-against-voice reuse at four slots is confirmed collision-free
> with beacons silenced, and three slots fails on reuse alone — but the supporting evidence is
> weaker than stated and the reuse margin rests on an uncited 10 dB capture threshold that,
> below 9.1 dB, reverses the result entirely.

**Spatial reuse requires at least four.** The originator transmits every frame, so with N
slots the radio N hops down the chain transmits in the same slot at the same instant. A
relay listening to its neighbour one hop away therefore hears that reuser N−1 hops away,
and the wanted signal must beat it by the demodulator's capture margin — about 10 dB for
4FSK. In open terrain the ratio is `10 × exponent × log₁₀(N−1)`: **15.3 dB at four slots,
9.6 dB at three.** Three is 0.4 dB short, and 48.5% of payloads die.

**The voice payload permits at most four.** At five slots a 12 ms slot carries 211 bits on
air, and sync plus header plus Codec2 3200 need 258. The voice does not fit at all, before
any error correction.

## Consequences

- The MAC frame structure is settled: 60 ms, four slots of 15 ms, one hop per slot.
- **The slot budget is not settled, and this decision makes it worse.** Four slots leaves
  6 bits for FEC — 2% where DMR runs 47%. See below.
- Concurrent calls when clustered: four, matching DMR's spectral efficiency
  ([OQ-0017](../open-questions.md#oq-0017)).
- Beacon airtime 9.0% rather than three slots' 12.1% ([OQ-0004](../open-questions.md#oq-0004)),
  so the cheaper option on that axis too.
- Reuse margin is 5.3 dB above the capture requirement. That is a real engineering margin
  rather than the 0.4 dB deficit at three slots.

### What this does not resolve

The budget can now only be closed by changing something outside the MAC, and the levers
are exactly the two the brief listed as open decisions:

| Gross rate | Vocoder | FEC available | |
|---|---|---|---|
| 19.2k | Codec2 3200 | 6 bits (2%) | as specified — unusable |
| 19.2k | Codec2 2400 | 54 bits (29%) | marginal; costs hard requirement 7 |
| 22.4k | Codec2 3200 | 50 bits (21%) | still thin |
| **22.4k** | **Codec2 2400** | **98 bits (52%)** | comfortable |
| **25.6k** | **Codec2 3200** | **94 bits (40%)** | comfortable, but see below |

25.6 kbps at 4FSK in 25 kHz is 0.51 symbols/Hz against DMR's 0.38 — 33% tighter filtering,
with adjacent channel power the thing EN 300 113 measures most closely. 22.4 kbps is
0.45 symbols/Hz, 17% tighter, and more plausible.

**So the project's critical path is now [OQ-0001](../open-questions.md#oq-0001): what gross
bit rate the CC1200 actually achieves in a 25 kHz channel.** That cannot be simulated. It
is the first measurement to make when hardware arrives, and Phase 0 has done what it was
for — it has said what to buy hardware to find out.

## Reversal trigger

The measured CC1200 bit rate comes in below ~19 kbps, at which point four slots cannot
carry Codec2 at any rate and the frame structure must be rebuilt around a 120 ms
superframe or a narrower vocoder.

Note also that the reuse result rests on two estimated numbers — a 10 dB capture threshold
and a 3.2 path-loss exponent. At 9 dB capture, or an exponent of 3.5, three slots would
pass. Both become measurable in Phases 1 and 2, and
[OQ-0019](../open-questions.md#oq-0019) records a modelling limitation that could make the
real case worse rather than better. Four slots is the safe side of a call that is closer
than it looks.

## Alternatives rejected

- **Three slots of 20 ms.** Closed the budget, failed spatial reuse in open terrain by
  0.4 dB. Was the leading candidate for most of Phase 0.
- **Two slots of 30 ms.** Ample payload, and a reuse distance of two — the hidden-terminal
  case exactly.
- **Five or six slots.** Excellent reuse margin, and the voice payload does not fit.
