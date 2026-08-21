# Feature set — a digital radio system that runs over a mesh

**Status:** Framing document. Defines what the product is, so that design decisions can be checked
against it.
**Relationship to the brief:** extends [Addendum 01](addendum-01-packet-architecture.md) in the same
direction. Nothing in the RF, MAC, routing or voice layers changes.

---

## What this product is

A professional digital mobile radio system with dispatch-grade features. The mesh replaces the
repeater as the transport. It does not replace the feature set.

That is the frame to design against. The comparison is not "a mesh that carries voice" — it is a
DMR deployment with a dispatch platform of the TRBOnet class, where the thing carrying the traffic
happens to be the handsets themselves rather than a licensed repeater on a mast.

The consequence is that features like private call, confirmed private call, radio check, call alert,
text with delivery receipt and emergency acknowledgement are **baseline expectations**, not future
enhancements. Their semantics are the established PMR ones. A private call is a selectively
addressed call, exactly as it is through a repeater — the repeater carries the audio there, and
relaying handsets carry it here, and in neither case does that change what the feature is called or
what it does.

## Scope guard

Listing a feature here is not committing to build it now. This document exists so that the frame
format, addressing and dispatch structure can accommodate the feature set — the thing
[Addendum 01](addendum-01-packet-architecture.md) exists to protect. Build order is a separate
decision.

The capacity discipline in Addendum 01 §7 still binds absolutely: voice always wins, data is
best-effort and rate-limited, and anything needing sustained throughput does not belong on this
system.

Two apparent conflicts with [brief §10](engineering-brief.md#10-things-that-are-not-in-scope),
resolved:

- *"Voice only"* was already amended by Addendum 01 §6, which explicitly enables text, position,
  logging, man-down and remote configuration. This document extends the same logic to the rest of
  the feature set.
- *"No trunking, registration, or centralised control of any sort"* still holds. Presence here is a
  by-product of the routing tables, not a registration protocol — see below. And a dispatch console
  is not centralised control: the mesh routes without it, and if the gateway dies the handsets carry
  on unaffected. Nothing in the network depends on a centre.

## The feature set

| Feature | Addressing | Confirmed? | Status |
|---|---|---|---|
| **Voice** | | | |
| Group / talkgroup call | Group address | No | Addressing built |
| Private (individual) call | Individual address | Optional | Addressing built; call control not |
| All call | Broadcast | No | Addressing built |
| Emergency call | Group or broadcast, priority 0 | Yes, ack expected | Frame type + priority defined |
| Late entry (join a call in progress) | — | — | **Not designed** — see [OQ-0015](open-questions.md#oq-0015) |
| Hang time (channel held for the reply) | — | — | Not designed; interacts with [OQ-0009](open-questions.md#oq-0009) |
| **Command and control** | | | |
| Radio check | Individual | Yes | Not designed |
| Call alert / page | Individual | Yes | Not designed |
| Remote monitor | Individual | Yes | Not designed; see [OQ-0014](open-questions.md#oq-0014) |
| Radio disable / enable | Individual | Yes | Not designed; see [OQ-0014](open-questions.md#oq-0014) and the note below |
| **Data** | | | |
| Text message | Individual or group | Optional, receipt | Frame type defined |
| Position report | Individual or group | Usually not | Frame type defined |
| Man-down / lone worker alarm | Group or broadcast, priority 0 | Yes | Frame type defined |
| Job ticketing | Individual | Yes | Not designed |
| Remote configuration | Individual | Yes | Frame type defined |
| **Dispatch (gateway-side)** | | | |
| Dispatch console | — | — | Gateway application; no protocol impact |
| Presence / who is online | — | — | **Free from the routing layer** — see below |
| Voice logging and playback | — | — | Gateway application; coverage caveat below |
| Telephone interconnect (SIP) | — | — | Gateway application, Codec2 ↔ G.711 |
| Talkgroup patching | — | — | Gateway application |

## Confirmed operations

Most of the command-and-control feature set is a request/response transaction with an
acknowledgement: radio check, call alert, stun and revive, text delivery receipt, emergency
acknowledgement, confirmed private call setup. Getting this right matters more here than on a
repeater, because the round trip is longer and the path can change underneath it.

**Where the transaction state lives: the payload, not the header.** A transaction identifier and an
acknowledgement flag are endpoint state. The relaying nodes do not need them and
[ADR-0007](decisions/0007-packet-switched-frame-architecture.md) forbids the routing layer from
reading payload. So confirmed operations need a control-payload format carrying a transaction ID
and a response code — and they cost **zero header bits**, which given
[OQ-0002](open-questions.md#oq-0002) is worth stating plainly before anyone panics about it.

The header's sequence number is not this. It is the relay layer's duplicate-suppression window and
has nothing to do with matching a response to a request end to end.

Three rules that follow:

1. **An acknowledgement inherits the priority of what it acknowledges.** An emergency ack is
   priority 0. A text delivery receipt is priority 3 and must never delay voice.
2. **Confirmed group operations do not exist.** Twelve radios cannot all acknowledge. Group and all
   calls stay unconfirmed, as they are in DMR.
3. **Every confirmed transaction is a round trip that occupies slots along the whole path, in both
   directions.** At five hops each way this is ten hop-times of channel occupancy for one radio
   check. Cheap individually, and the reason Addendum 01 §7's rate limiting is load-bearing rather
   than decorative.

## What the mesh gives you for free

**Presence.** A conventional deployment needs a registration service so the dispatcher knows which
radios are on. Here, neighbour discovery and topology control already maintain that — every node
knows who is reachable, and a gateway participating in the mesh has the whole picture without
asking anyone. Presence is a read of the routing table.

**Topology awareness.** More than a repeater system can offer: the dispatch console can show not
just who is online but how the group is connected — who is relaying for whom, who is one hop from
falling off the end of the chain. For a leader supervising a dispersed group that is arguably a
better situational picture than a position list, and it costs nothing to produce.

This is also the honest answer to why [OQ-0011](open-questions.md#oq-0011) closed the way it did.
Topology control looked like overhead to be eliminated; under this framing it is a feature.

## What is harder on a mesh than on a repeater

**Distributed pre-emption.** A repeater arbitrates. Here, an emergency frame pre-empting an
in-progress call has to be honoured independently by every node along the path, with no referee.
Part of [OQ-0009](open-questions.md#oq-0009), and the hardest part of it.

**Complete voice logging.** A gateway logs what it hears. At the regular meeting point with a
licensed fixed site, that is everything. Off-site — hills, woodland, a splinter group over a ridge —
a gateway cannot be there at all, so logging is inherently partial in exactly the conditions the
product exists for. Not a protocol problem; a limitation to be honest about in the product
description rather than discovered later.

**Transactions across a moving path.** A confirmed operation assumes the route survives long enough
for the response to come back. At walking pace over several hops that is not guaranteed. Needs a
timeout and retry policy that does not turn one lost ack into a broadcast storm. See
[OQ-0016](open-questions.md#oq-0016).

**Command authentication.** On a repeater system, a stun command reaches the radio through
infrastructure the operator controls. On an infrastructure-free mesh any node can originate any
frame, so an unauthenticated disable command is a weapon that disables the group's radios. See
[OQ-0014](open-questions.md#oq-0014).

> **A judgement call worth making early, not late.** For this operator — a youth organisation whose
> radios exist for supervision and emergency escalation — a remotely disable-able handset is
> arguably a liability rather than a feature. The failure mode is a leader in difficulty whose radio
> has been silenced, by accident or by someone who should not have been able to. Radio check and
> call alert carry no such risk and are straightforwardly useful. Recommend supporting disable/enable
> in the architecture and shipping it switched off, or omitting it. Not a decision to default into.

## What is unchanged

The RF layer, the 25 kHz channel, 4FSK, the TDMA frame, slot pipelining, MPR routing and Codec2
3200. Every feature above is carried by the frame architecture already decided in
[ADR-0007](decisions/0007-packet-switched-frame-architecture.md). Nothing here reopens the MAC.

That is the addendum working as intended.
