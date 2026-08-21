# Open questions

Live register of undecided items. Each has an owner phase — the earliest phase at which it can
actually be answered — and a statement of what it blocks. When one closes, it becomes an ADR (or an
amendment to an existing one) and is struck through here rather than deleted.

Items OQ-0001 and OQ-0003 through OQ-0008 come from [engineering brief §8](engineering-brief.md#8-open-decisions).
The rest surfaced while writing the decision log and are not yet in the brief.

| # | Question | Answerable in | Blocks | Status |
|---|---|---|---|---|
| [OQ-0001](#oq-0001) | Achievable gross bit rate at 25 kHz on CC1200 | Phase 1 | Slot count, vocoder rate, FEC strength | Open |
| [OQ-0002](#oq-0002) | The slot budget does not close as specified | **Phase 0** | Everything downstream of the MAC | Open |
| [OQ-0003](#oq-0003) | Synchronisation: GPS-disciplined or network-derived | Phase 0 (design), Phase 2 (proof) | Guard interval size, canopy/indoor operation | Open |
| [OQ-0004](#oq-0004) | Beacon interval, and where control traffic lives in the slot structure | **Phase 0** | Channel overhead, reconvergence time | Open |
| [OQ-0005](#oq-0005) | Slot count — is 4 right? | Phase 0, revisit Phase 1 | Hops per frame, per-slot payload | Open |
| [OQ-0006](#oq-0006) | VHF Mid Band or High Band | Ofcom enquiry | Nothing technical | Open |
| [OQ-0007](#oq-0007) | Encryption | Phase 3 | BOM, key management UX | Open |
| [OQ-0008](#oq-0008) | In-house or contracted development | Commercial | Schedule and cost | Open |
| [OQ-0009](#oq-0009) | Channel access: who owns a slot, and what happens on simultaneous PTT | **Phase 0** | The MAC is not fully specified without this | Open |
| [OQ-0010](#oq-0010) | RX→TX turnaround budget on a half-duplex transceiver | Phase 1 | Guard interval, therefore payload, therefore OQ-0002 | Open |
| [OQ-0011](#oq-0011) | Is full OLSR topology-control dissemination needed at all? | Phase 0 | Roughly half of routing overhead | Open |

---

## OQ-0001
### Achievable gross bit rate at 25 kHz with the CC1200

The 19.2 kbps figure is scaled from DMR's proven 9.6 kbps in 12.5 kHz — doubling the channel and
assuming equivalent spectral efficiency at 4FSK. It is an assumption, not a measurement, and
[ADR-0002](decisions/0002-tdma-slot-pipelining.md), [ADR-0004](decisions/0004-codec2-3200.md) and
the entire slot structure sit on top of it.

Cannot be closed before hardware. Until then, Phase 0 should treat gross rate as a **parameter**,
not a constant, and report results across a range — at minimum 16.0, 19.2 and 22.4 kbps — so that
whatever the CC1200 turns out to deliver, the answer is already on the shelf.

## OQ-0002
### The slot budget does not close as specified

**The most consequential open item in the project.** From
[ADR-0002](decisions/0002-tdma-slot-pipelining.md):

| Quantity | Value |
|---|---|
| Raw bits per 15 ms slot at 19.2 kbps | 288 |
| Less guard at DMR-equivalent proportion (2.5 ms in 30 ms = 8.3%) | −24 |
| Usable payload | ~264 |
| Required: Codec2 3200 over 60 ms (192 bits) + 47% FEC and framing | 282 |
| **Deficit** | **~18 bits (~7%)** |

Guard cannot simply be trimmed. It has to cover transmitter attack and release — which
[EN 300 113 tests directly, via adjacent channel power during burst transitions](engineering-brief.md#7-regulatory-constraints)
— plus RX→TX turnaround ([OQ-0010](#oq-0010)) and sync uncertainty ([OQ-0003](#oq-0003)).
Propagation delay is negligible: 50 µs at 15 km.

Four candidate resolutions, each of which changes something currently marked "decided":

1. **Raise the gross rate** to ~20.5 kbps. Depends entirely on [OQ-0001](#oq-0001) and is not in
   our gift.
2. **Cut FEC and framing** from 47% to ~37.5%. The 47% is inherited from DMR's ratio, not derived
   from this channel's error characteristics — so it may simply be the wrong number rather than a
   constraint. Requires the 4FSK BER curve at working SNR, which is unmeasured.
3. **3 slots of 20 ms.** 384 raw bits, ~352 after guard — comfortable. Costs one pipelined hop per
   frame (2 rather than 3), pushing 5-hop latency up. Check against the 300 ms criterion before
   dismissing; it may still pass. Interacts with [OQ-0005](#oq-0005).
4. **120 ms voice superframe.** One codec payload spans two slot opportunities. Preserves 4 slots
   and the gross rate, at the cost of added end-to-end latency and a more complex framing layer.

Phase 0 should evaluate all four rather than pick one on paper. This is the first thing the
simulator earns its keep on.

## OQ-0003
### Synchronisation method

GPS-disciplined is simplest and is the Phase 0/1 assumption. Network-derived sync is more elegant,
works under canopy and indoors — both of which are real operating conditions for this group, not
edge cases — and is considerably harder.

Bears directly on [OQ-0002](#oq-0002): GPS gives sub-microsecond alignment and a small sync
allowance within the guard interval. Network-derived sync is looser and costs guard time, which
costs payload.

Decide the Phase 0/1 approach now (GPS), but do not design the frame structure in a way that makes
network-derived sync impossible to retrofit.

## OQ-0004
### Beacon interval, and where control traffic lives in the slot structure

Two questions that have been treated as one, and are not:

**Interval** is the stated trade-off between reconvergence speed and channel overhead. Standard OLSR
HELLO at 2 s and TC at 5 s was designed for links three orders of magnitude faster than this one and
should not be inherited. Phase 0 should sweep the interval and report reconvergence time against
overhead as a percentage of channel capacity — one of Phase 0's five required answers.

**Placement is not specified anywhere in the brief.** Beacons and topology control contend with
voice for the same four slots. The options — a dedicated control slot, stealing an idle voice slot,
piggybacking on voice frames, or a periodic control superframe — have materially different costs,
and the choice interacts with [OQ-0005](#oq-0005) and [OQ-0009](#oq-0009). Cannot ship a MAC
without answering it.

## OQ-0005
### Slot count

Four is derived from Codec2 3200 plus 47% FEC at 19.2 kbps — and that derivation is exactly the one
that does not close ([OQ-0002](#oq-0002)). Revisit once the gross rate is measured.

Note the coupling: slot count sets both per-slot payload (more slots, less payload each) and hops
per frame (more slots, more pipelined hops). These pull in opposite directions and the optimum
depends on the real gross rate and the real dispersal geometry.

## OQ-0006
### VHF Mid Band or High Band

Propagation is near-identical. Availability of a 25 kHz simplex assignment decides it. This is an
Ofcom enquiry, not an engineering question, and nothing downstream waits on it — but it should be
raised early, because it is also where the answer to "is a 25 kHz simplex assignment obtainable at
all" comes from, and that one *is* load-bearing
([ADR-0001 reversal trigger](decisions/0001-narrowband-vhf-licensed-spectrum.md#reversal-trigger)).

## OQ-0007
### Encryption

Not required. AES is cheap to add and is expected in this class of product. The real cost is not the
cipher, it is key management on kit that is shared between volunteers with minimal training — which
collides with hard requirement 8. Defer to Phase 3, but reserve the header bits now.

## OQ-0008
### In-house or contracted development

Commercial, not technical. Recorded here for completeness.

## OQ-0009
### Channel access: who owns a slot, and what happens on simultaneous PTT

**A gap in the specification, not just an undecided parameter.** The brief defines a TDMA frame and
a pipelining rule, but never says how a node acquires the right to originate in a given slot.

Unanswered: which slot does an originator start in? If every originator starts in slot 0, what
happens when two leaders press PTT in the same frame? Is there a contention or reservation
mechanism, or is it first-heard-wins with the loser's audio lost? How does a node learn a
transmission is in progress and defer?

This also determines capacity under multiple talkers. A 4-slot frame carrying one pipelined stream
uses all four slots for that stream; a second simultaneous talker has nowhere to go without either
halving the pipeline depth or adding a frame's delay.

Conventional simplex resolves this socially — you hear that someone is talking and you wait. That
may well be the right answer here too, and it fits requirement 8. But it needs to be a decision with
a mechanism behind it, not an omission.

## OQ-0010
### RX→TX turnaround budget on a half-duplex transceiver

Slot pipelining requires a relay to receive in slot *n*, decide, and transmit in slot *n+1* — the
turnaround happening entirely inside one guard interval. The CC1200's real RX→TX switching time,
plus PLL settling, plus the ramp shaping that EN 300 113 compliance demands, is unmeasured.

If it does not fit, [ADR-0002's reversal trigger](decisions/0002-tdma-slot-pipelining.md#reversal-trigger)
fires. Measure it first in Phase 1, before anything else is built on the bench.

## OQ-0011
### Is full OLSR topology-control dissemination needed at all?

OLSR carries two message classes: HELLO for neighbour discovery, and TC for link-state
dissemination so nodes can compute routes to arbitrary destinations.

But this network's traffic is one-to-many voice on a broadcast medium — every transmission is
intended for the whole group. If MPR-optimised flooding alone delivers that correctly, TC messages
compute routes nobody uses, at roughly half the total routing overhead. On a 19.2 kbps channel that
is worth confirming rather than assuming.

Caveat before deleting TC: [required behaviour §4.4](engineering-brief.md#44-required-behaviour)
mentions splinter groups routing "through whoever is between", and any future move toward addressed
or private calls would need real routes. Confirm scope before removing it.
