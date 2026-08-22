# ADR-0013: Take VINE's header-inspection routing. Leave its MAC and its power model.

**Status:** Accepted (decision recorded; implementation is W-06b, Phase 1+)
**Date:** 2026-08-21
**Phase:** Decided in Phase 0, built later
**Source:** [gotenna-vine.md](../gotenna-vine.md)

## Context

goTenna's VINE builds routing state by reading the headers of ordinary data packets rather
than by exchanging dedicated control messages. They are in under 25 kHz, they ship it, and
their own accounting says the entire extra cost of the protocol is one header field, because
everything else it needs is already present in any MANET stack.

This ADR exists to **close the question** rather than leave a promising reference open. It
records what is taken, what is not, and when.

## Decision

### Take: the routing mechanism

**A data packet already carries almost everything a routing protocol needs.** We are one
8-bit field — the sender's own previous hop — from being able to build distance gradients to
every node we hear, with no control traffic at all.

We got most of the way by accident. `prev` was added for
[OQ-0018](../open-questions.md#oq-0018), because the forwarding rule had to test the sender of
a copy rather than its origin. That is exactly VINE's `sender` field.

**Why it matters more than the airtime it saves:** [W-04](../backlog.md), Controlled Barrage
Regions, needs every node to know its hop distance from both talker and listener so a call can
be confined to a corridor instead of flooding the network. Those distances are precisely what
gradients are. The highest-value unbuilt item on the backlog acquires a published, measured,
shipped mechanism for the price of one field.

### Take: "no control packets" — but only the half that applies

Right while there is traffic to read. Voice at a 160 ms frame produces about 375 payloads a
minute against VINE's ~4-per-minute break-even, so gradients would be trivially fresh during a
talkspurt.

**Wrong in silence**, where there is nothing to infer from. So beacons stay — but their rate
can fall a long way, and they currently cost 9.0% of slots
([OQ-0004](../open-questions.md#oq-0004)).

### Leave: the MAC

G-CSMA, and its planned replacement SPIN. [ADR-0002](0002-tdma-slot-pipelining.md) rejected
CSMA because it cannot bound latency and hidden-node collisions are the normal case in a
dispersed mesh. Nothing in VINE disturbs that. **Their traffic can be four seconds late and
nobody notices; ours cannot be half a second late.**

### Leave: sleep-wake power management

A radio that sleeps is not relaying. In a network where every handset is a relay, sleep is a
topology change rather than a power setting — the same conclusion reached about radio silence
in [nbwf-lessons.md §7](../nbwf-lessons.md).

## Consequences

- **W-06b** added to the backlog: carry the extra field, build gradients in the C core.
  Prerequisite for W-04. Costs 8 bits, taking FEC from 16% to 14.6%.
- **Not Phase 0 work.** Phase 0's criteria are met without it, and it changes the wire format
  — which is the one artefact [ADR-0001](0001-narrowband-vhf-licensed-spectrum.md) notes
  cannot change after radios ship. It wants doing deliberately, alongside W-04, not bolted on
  at the end of a simulation phase.
- We independently built their per-hop reliability mechanism already: `manet_sched_heard` and
  `manet_sched_suppress` are VINE's implicit acknowledgement. Their caveat — that it fails
  with directional antennas — does not apply to helicals.
- The brief's rejection of goTenna Pro X2 ("carries no voice") is weakened by their Oct 2023
  voice demonstration and needs re-checking. That is **H-06**, and it is somebody else's few
  hours rather than a protocol question.

## Alternatives rejected

- **Adopt Aspen Grove wholesale.** Its MAC and power model are built for messaging and would
  cost the only property we have that they have not published: bounded conversational latency.
- **Take nothing, on the grounds that our beacons already work.** They work and they cost 9%
  of slots, and refusing a mechanism that is one field away because the current one functions
  is how a design accumulates avoidable overhead.
- **Build it now.** It changes the wire format. See *Consequences*.
