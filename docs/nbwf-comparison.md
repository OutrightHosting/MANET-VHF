# NBWF — the same system, standardised, and what it tells us

> **Superseded in scope by [nbwf-lessons.md](nbwf-lessons.md), 2026-08-21.** This document
> was written from a partial read that extracted the per-burst overhead figure and little
> else. The full read found material that changes decisions — FFI's retreat from automatic
> relaying, their CPM physical layer, their γ = 4 propagation exponent against our 2.97, and
> answers to five of our open questions. Read that one first; this remains accurate on the
> overhead comparison.

**Source:** Jodalen, Solberg & Haavik, *NATO Narrowband Waveform (NBWF) — overview of link
layer design*, FFI-rapport 2009/01894, Norwegian Defence Research Establishment, 28 March
2011. Read in full. PDF kept alongside this file.

NBWF is a NATO standardisation effort for **a single-channel MANET carrying voice and data
simultaneously over a 25 kHz VHF/UHF channel**. That is our specification, word for word,
being written as a STANAG by NC3A, CRC Canada, FFI and Kongsberg since 2007.

Their §2.2 list of TDMA requirements is our design goals verbatim: *"be fully distributed,
i.e. no master node used"*, *"efficient operation in multi-hop as well as single-hop"*,
*"provide a rapid set-up of voice time slots after a push-to-talk operation"*, *"pre-empting
lower priority traffic"*, *"share available data capacity on a fair basis"*.

And their conclusion on channel access is the one this project reached the hard way: because
voice delay is tight and *"difficult to achieve for a stochastic MAC protocol (e.g. a random
access protocol), it was decided that the MAC protocol should be based on a TDMA structure
with a suitable reservation mechanism."*

---

## 1. The finding that matters most: our per-burst overhead is out by six times

NBWF's physical layer burst carries an acquisition and signalling preamble before any data:
sync preamble 1.5 ms, Start-Of-Message 2.1 ms, Par field 1.6 ms, transition 0.1 ms. §4.3
puts the total at **approximately 8 ms** and uses that figure throughout its budget.

**`MANET_SYNC_BITS` in our `config.h` is 24.** At 19.2 kbps, 8 ms is **154 bits**.

The consequence is not marginal. It removes the frame:

| Slot | Data time after guard and preamble | Bits | Header + voice needs 234 |
|---|---|---|---|
| **15 ms (ours)** | 5.8 ms | **110** | **does not fit** |
| 22.5 ms (NBWF's) | 12.6 ms | 243 | fits, 8 bits for FEC |
| 26 ms | 15.8 ms | 304 | fits, 70 for FEC |
| 30 ms | 19.5 ms | 375 | fits, 140 for FEC |

A 15 ms slot cannot carry Codec2 3200 at all once a realistic preamble is paid for. Not
"cannot carry it with adequate FEC" — cannot carry the header and payload.

**And it explains NBWF's frame.** The preamble is a fixed cost per burst, so short slots are
dominated by it. NBWF chose 22.5 ms slots precisely to amortise the 8 ms, and says so: *"one
transmission burst will have to comprise several MELPe frames. The more MELPe frames packed
in a burst, the higher efficiency will be obtained."*

This is a tension [ADR-0002](decisions/0002-tdma-slot-pipelining.md) never saw. **Pipelining
wants short slots for low per-hop latency; acquisition overhead wants long ones.** Our
15 ms slot optimises one and is destroyed by the other.

## 2. Their frame, and ours

| | NBWF | MANET-VHF |
|---|---|---|
| Channel | 25 kHz | 25 kHz |
| PHY rate | 20 kbps (N1 mode) | 19.2 kbps (assumed, unmeasured) |
| Frame | **202.5 ms** | 60 ms |
| Slots | **9 × 22.5 ms** | 4 × 15 ms |
| Per-burst preamble | **8 ms** | 24 bits (1.25 ms) |
| Vocoder | MELPe 2400 | Codec2 3200 |
| Slots per voice call | **2 per frame** | 1 per frame |
| Cost of a relay | **+2 slots per relay** | 1 slot per hop, pipelined |
| Voice channel access | **RTS + multi-CTS per talkspurt** | none |
| Relaying | **dedicated, configured** | automatic, every node |

Their voice budget: 202.5 ms buffering + 37 ms transmission + 45 ms algorithmic delay ≈
**300 ms on a half-duplex link without relaying**, against a requirement of 250 ms
(§4.2, [4] para 2.01.08). Relaying adds n × 22.5 ms on top. They are already over budget
before a single relay.

Of nine slots: two for voice, two per relay. One relay leaves five slots for data; two relays
leave three. §4.5 concludes plainly: *"Relaying of voice is very expensive."*

## 3. The humbling part

NBWF considered exactly our architecture — every node relays, automatically, no configuration
— and did not attempt it. §4.4, verbatim:

> *"Automatic relaying of voice in a multihop environment without the need for dedicated relay
> nodes is a highly desirable feature that the NBWF MAC protocol should aim for. This
> functionality is not available in any CNR subnets today... Automatic relaying will be a very
> complex protocol due to the real time delay requirements, limited bandwidth and the mobile
> environment. **We are not sure if such a protocol is feasible within 25 kHz bandwidth**, and
> extensive simulations are needed to provide the answer. In order to arrive at a draft MAC
> protocol specification earlier, we propose a simpler solution with dedicated relay(s) for
> voice."*

A NATO standardisation team, writing the link layer for this exact channel, deferred the
thing this project treats as its premise — and were not sure it could be done at all.

That is not a reason to stop. It is the gap in the market the brief identified, and being
hard is why it is still there. But it must be stated: **the central claim is one that the
relevant standards body flagged as possibly infeasible in 25 kHz and did not attempt.**

## 4. What to take

**Adopt: RTS + multi-CTS per talkspurt.** §4.2's mechanism is the answer to
[OQ-0009](open-questions.md#oq-0009), which has been the largest open item for the whole of
Phase 0. The PTT action triggers a signalling sequence that allocates slots for the sender
*and its relays*; at the end of the talkspurt they are released. Crucially: *"The signalling
procedure which consists of an RTS signal from the sender node and a number of CTS signals
from one-hop neighbours, also ensures that the **two-hop neighbours** of the sender node
become aware of the ongoing talk spurt, and make them refrain from transmitting in the
occupied time slots."* That is the hidden-terminal fix, and it is why a chain can carry two
conversations without the deafness measured in [OQ-0021](open-questions.md#oq-0021).

The cost is access time — the 250 ms PTT-to-allocation budget they call *"a value that may be
hard to achieve in some subnet topologies."*

**Adopt: a superframe for fair access.** §4.5 — a node cannot own a slot in every frame once
the subnet exceeds ~10 nodes, so fixed access rotates across a superframe. This is what the
literature review said and what our own superframe experiment failed to find a use for. NBWF
uses it for the guaranteed minimum capacity C_min, not for beacons.

**Re-open: slot and frame duration.** [ADR-0008](decisions/0008-four-slots.md) fixed four
slots from spatial reuse and payload. Both arguments assumed 24 bits of preamble. With 154
the payload argument inverts completely, and the frame must lengthen — which is precisely
what NBWF did.

**Verify first: the real preamble on our hardware.** The 8 ms is NBWF's own PHY, not ours. A
CC1200 in a simpler mode may need far less. But 24 bits was a guess with nothing behind it,
and the only comparable published figure is six times larger. This is now the most valuable
single measurement in Phase 1, ahead of the gross bit rate — because if it is anywhere near
8 ms, the frame structure is wrong rather than merely tight.
