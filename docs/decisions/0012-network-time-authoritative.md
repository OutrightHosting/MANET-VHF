# ADR-0012: The network is the clock. GPS is advisory.

**Status:** Accepted (design); implementation not started
**Date:** 2026-08-21
**Phase:** Phase 0 design, Phase 1 measurement
**Relates to:** [OQ-0031](../open-questions.md#oq-0031), [OQ-0003](../open-questions.md#oq-0003),
[OQ-0024](../open-questions.md#oq-0024), [OQ-0028](../open-questions.md#oq-0028)

## Context

Every timing advantage this design holds over [NBWF](../nbwf-lessons.md) came from being
allowed to depend on GNSS where FFI were not. That was defended on threat model: their
adversary jams GPS, ours is a wet hillside.

**That defence does not survive the question "what if GPS goes down".** It is a probability
argument about a failure whose consequence is the total loss of a safety system, and those do
not trade against each other. GNSS denial is not exotic — jamming is routine near conflict
zones and increasingly reported across European airspace, spoofing incidents are documented,
and a constellation-level or solar event needs no adversary at all. A system for keeping
twelve leaders in contact with each other cannot have a single point of failure in a service
nobody involved operates.

**Spoofing is the worse case and it inverts the design.** A jammed receiver reports no fix. A
spoofed one reports a confidently wrong one. If nodes trust GPS absolutely, a single spoofed
radio transmits in the wrong slot and jams the network — so blind trust in GPS is *more*
dangerous than having no GPS at all.

## Decision

**Network time is authoritative. GPS is an input to it, not the source of it.**

The mechanism is already in the architecture and was adopted three commits ago without anyone
noticing what else it does. [Glossy](http://www.olgasaukh.com/paper/ferrari11glossy.pdf)
(Ferrari, Zimmerling, Thiele & Saukh, IPSN 2011) is titled *"Efficient Network Flooding **and
Time Synchronization**"*, and reports an average synchronisation error **below one
microsecond** — obtained *implicitly*, as a byproduct of the flood itself, with no sync
traffic, no master and no external reference.

In a barrage flood every node transmits in a slot deterministically related to the
originator's. So **receiving a flood tells a radio what time it is**, to within the accuracy
of its burst-edge detection. We flood continuously during a call, and beacons flood between
calls, so the sync pulse is free and constant. [ADR-0011](0011-barrage-relaying.md) bought
this and only paid for the flooding.

Four layers, of which only the third exists today:

1. **Cold start, no GPS anywhere.** Listen one superframe. If nothing is heard, transmit a
   beacon at a randomised offset and listen again. Converge by consensus. **This is the hard
   part and it is genuinely unbuilt.**
2. **Running, no GPS.** Every received burst is a timing reference. Steer toward the
   neighbourhood consensus. No master — which also satisfies the no-master requirement that
   rules out a designated time source.
3. **GPS at some nodes.** Their estimate is weighted higher but **sanity-checked against
   network consensus, and rejected if it disagrees beyond a threshold.** This is the
   anti-spoofing rule and it falls out of making the network authoritative.
4. **GPS everywhere.** Today's design. Fastest acquisition and the tightest preamble.

## Consequences

- **The accuracy requirement is undemanding.** `MANET_GUARD` is 3320 µs and that is the whole
  error budget. Glossy measures under 1 µs — **3320× inside it**. Our PHY is slower and
  narrowband and Glossy's constructive-interference mechanism at 802.15.4 rates will not
  transfer intact, so assume we do far worse:

  | Sync error | Margin on the 3320 µs guard |
  |---|---|
  | Glossy, < 1 µs | 3320× |
  | 100× worse — 100 µs | 33× |
  | 1000× worse — 1 ms | 3.3× |

  Even three orders of magnitude worse than the published figure still fits. **The mechanism
  transfers; the accuracy does not need to.**
- **Frequency is the harder half and is not solved by this ADR.**
  [OQ-0024](../open-questions.md#oq-0024) and [OQ-0028](../open-questions.md#oq-0028) need a
  disciplined *reference*, not just a disciplined *clock*. The same bursts that carry timing
  also carry a measurable carrier frequency offset, so network frequency lock is the same
  idea applied to a different quantity — and a network locked to itself may hold tighter
  mutual CFO than free-running oscillators would. **Plausible, unproven, and it decides
  whether the preamble stays at 56 bit-times or returns to 128.**
- **GPS keeps three real jobs**: cold start in seconds rather than tens of seconds, absolute
  time for incident logs, and tightening the estimate while it is trusted.
- **Network merge is a new failure mode.** Two islands that never heard each other will each
  reach an internally consistent consensus, and those will differ. When they meet, one must
  yield. Needs a deterministic tie-break — and it is the same problem as
  [OQ-0015](../open-questions.md#oq-0015) late entry, approached from the other side.
- **Nothing here is free of implementation risk.** Layer 1 is real work, and layer 2's loop
  must not oscillate or let a badly-drifting node drag the network with it.

## Reversal trigger

Phase 1 measurement shows burst-edge timing on the CC1120/CC1200 cannot resolve better than
~1 ms, at which point network-derived sync cannot hold the guard and GPS returns to being a
dependency — in which case the honest response is to say so in the product documentation
rather than to hide it.

## Alternatives rejected

- **Elected time master.** Simple and it works. Rejected because it reintroduces the single
  point of failure the user explicitly ruled out for this network, one layer down.
- **Accept the GPS dependency and document it.** Defensible for a hobby system. Not for a
  safety-related one, and not when the mechanism that removes the dependency is already in
  the architecture and already paid for.
- **Free-running with periodic manual resync.** Fourteen minutes of holdover on a standard
  TCXO ([OQ-0031](../open-questions.md#oq-0031)) makes this an operational burden, not a
  design.
