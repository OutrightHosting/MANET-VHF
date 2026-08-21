# ADR-0007: Packet-switched frame architecture with typed, addressed frames

**Status:** Accepted
**Date:** 2026-08-21
**Phase:** Pre-development
**Source:** [Addendum 01](../addendum-01-packet-architecture.md)
**Amends:** [ADR-0002](0002-tdma-slot-pipelining.md) (payload arithmetic), [ADR-0003](0003-olsr-mpr-routing.md) (routing/payload separation)

## Context

The main brief specifies a voice system, which invites a design where voice *is* the payload and
everything else is bolted on later. Addendum 01 forecloses that: build a packet network that happens
to carry voice as its primary traffic type.

The cost of getting this wrong is asymmetric. A type field omitted to save four bits is a rewrite of
the MAC in six months; four bits spent now is four bits.

## Decision

**Every transmission is an addressed, typed frame. Voice is one frame type among several.**

The receive path is a dispatcher on frame type, not a voice decoder with exceptions. The routing
layer reads the header and never the payload.

### Header — straw-man field widths

Proposed, not settled. Field *presence* is fixed by the addendum; field *width* is a live fight with
the slot budget and is tracked as [OQ-0012](../open-questions.md#oq-0012).

| Field | Bits | Range / notes |
|---|---|---|
| Source address | 8 | Per-network, not globally unique |
| Destination address | 8 | Kind is implied by range — no separate flag bits |
| Frame type | 4 | 16 values, 8 reserved |
| Sequence number | 8 | 256-frame duplicate suppression window ≈ 15 s at 60 ms |
| TTL / hop count | 4 | 15 hops — matches the brief's stated maximum exactly |
| Priority | 2 | Exactly the four classes in Addendum 01 §5 |
| **Total** | **34** | |

### Addressing — straw-man map

Destination *kind* is encoded in the address range rather than in dedicated flag bits. This is the
frugality the addendum asks for: it buys individual, group, broadcast and reserved semantics for
zero extra header bits.

| Range | Meaning |
|---|---|
| `0x00` | Unassigned / null |
| `0x01`–`0x9F` | Individual handheld nodes (159) |
| `0xA0`–`0xBF` | Individual gateway and infrastructure nodes (32) |
| `0xC0`–`0xEF` | Groups / talkgroups (48) |
| `0xF0`–`0xFE` | Reserved — future node classes, well-known addresses |
| `0xFF` | Broadcast — neighbour discovery, beacons |

### Frame type — straw-man enumeration

Half the space is reserved. Everything below `0x8` except voice and signalling is defined now and
implemented later.

| Value | Type | Priority class |
|---|---|---|
| `0x0` | Voice | 1 |
| `0x1` | Voice terminator | 1 |
| `0x2` | Beacon / neighbour discovery | 2 |
| `0x3` | Topology control | 2 |
| `0x4` | Emergency / man-down | 0 |
| `0x5` | Text message | 3 |
| `0x6` | Position report | 3 |
| `0x7` | Remote configuration | 3 |
| `0x8`–`0xF` | Reserved | — |

### Node types

Handheld and gateway. A gateway participates in the mesh **exactly** as a handheld does — same MAC,
same routing, same slot discipline. The only differences are what it does with frames addressed to
it and what it originates. The routing layer must not be able to tell them apart.

### Priority

The four classes are implemented and honoured from the outset, even while only voice and signalling
exist. Voice is never queued behind data.

## Consequences

### The slot budget gets materially worse, and this is the headline

The brief's "~47% FEC and framing" is a blanket figure inherited from DMR. The addendum forces it to
be itemised, and itemising it is where the problem shows. At 4 slots × 15 ms × 19.2 kbps:

| | Bits |
|---|---|
| Raw bits per slot | 288 |
| Less guard at DMR-equivalent proportion (8.3%) | −24 |
| **On-air bits** | **264** |
| Less sync / preamble (24; DMR spends 48) | −24 |
| Less header (§ above) | −34 |
| Less Codec2 3200 per 60 ms frame | −192 |
| **Left for FEC** | **14** |

Fourteen bits. That is a CRC, not forward error correction. On a narrowband channel with no
retransmission and voice that must stay intelligible at the fringe, that is not a viable frame.

[OQ-0002](../open-questions.md#oq-0002) was a ~7% shortfall against a blanket figure. It is now a
structural one, and it is the first thing Phase 0 must resolve.

### The 3-slot option survives itemisation, and costs less than the brief implies

Running the same arithmetic at 3 slots × 20 ms:

| | Bits |
|---|---|
| Raw bits per slot | 384 |
| Less guard (8.3%) | −32 |
| Less sync / preamble | −24 |
| Less header | −34 |
| Less Codec2 3200 | −192 |
| **Left for FEC** | **102** |

102 bits protecting 226 — a 45% overhead ratio, which lands almost exactly on the brief's 47%
target. The budget closes.

Two observations, both **analysis awaiting simulation** rather than settled results:

**Latency is not the obstacle.** Under slot pipelining a payload advances one hop per *slot*, so
hop latency equals slot duration: 15 ms at 4 slots, 20 ms at 3. A 5-hop chain costs 75 ms or 100 ms
respectively, against a 300 ms criterion. Both pass with enormous margin. Note also that the brief's
own figure — "~20 ms per hop, 15 hops at 300 ms" — is arithmetically the *3-slot* case; at 4×15 ms
the real number is 225 ms.

**The actual cost of 3 slots is spatial reuse margin, not hop rate.** The originator emits a new
codec payload every frame and so transmits in slot 0 of every frame. With N slots, by the time it
sends payload *k+1*, payload *k* is being relayed by the node N hops away — in the same slot,
simultaneously. N is therefore the reuse distance. At N=4 that is comfortable; at N=3 it is
marginal, since 2-hop separation is exactly the hidden-terminal case. This is the trade to
characterise, and the brief frames it incorrectly as "hops per frame". Tracked as
[OQ-0013](../open-questions.md#oq-0013).

### Routing

- The routing layer reads header fields only — never payload. Priority is a header field, so
  pre-emption does not violate this.
- Duplicate suppression and MPR selection already operate on source/sequence, which the header now
  formalises rather than adds.
- **[OQ-0011](../open-questions.md#oq-0011) is largely closed against dropping topology control.**
  That question rested on all traffic being one-to-many broadcast voice, so that MPR flooding alone
  would suffice. Individual addressing, gateways as destinations, text and configuration frames all
  require real routes to specific nodes. TC stays.

### Licensing

The brief §7 argues at length that a carried handheld is not a fixed location and therefore not a
base station. That argument is untouched and still applies to handhelds. Gateways are a separate,
conventional case: a fixed installation with an antenna at a licensed site, which is a well-trodden
Technically Assigned licence rather than anything novel.

Worth being explicit about what this does *not* solve: a gateway needs a licensed fixed site, and
the operational problem in the brief is off-site — hills, woodland, moorland, where no site can be
licensed. Gateways serve the regular meeting point, which already has two licensed fixed sites. They
do not help on residential trips, which is where the mesh has to earn its keep on its own.

### Capacity

Data is best-effort and rate-limited. No video, no file transfer, no telemetry streaming. A feature
needing sustained throughput does not belong on this system. This is a standing constraint on scope,
not just an implementation note.

## Reversal trigger

None. This is a structural constraint, and the cost of reversing it is the rewrite it exists to
prevent. Field widths and enumeration values are expected to move — that is
[OQ-0012](../open-questions.md#oq-0012), not a reversal of this ADR.

## Alternatives rejected

- **Voice-first framing, packets bolted on later.** The default, and the thing the addendum exists
  to prevent. Costs a MAC rewrite.
- **Separate address-kind flag bits.** Two bits to say "this is a group address" when the address
  range can say it for free, on a channel where fourteen bits decides whether FEC exists.
- **Omitting the type field while only voice exists.** Saves four bits now, costs the header format
  and every deployed radio later.
