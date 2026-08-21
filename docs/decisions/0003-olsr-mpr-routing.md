# ADR-0003: OLSR-derived proactive routing with MPR flooding

**Status:** Accepted
**Date:** 2026-08-21 (recorded; decision predates the repository)
**Phase:** Pre-development

## Context

Voice is push-to-talk and latency-intolerant. A reactive protocol (AODV, DSR) discovers a route on
demand, which means the first syllable of every transmission pays a route-discovery round trip.
That is unacceptable for a radio whose users expect it to behave like simplex.

The network is small (≈12 nodes), the traffic is one-to-many broadcast rather than point-to-point,
and the topology changes at walking pace.

## Decision

A proactive, OLSR-derived design adapted to a slotted, narrowband, half-duplex channel:

- **Neighbour discovery** — periodic beacons; each node holds a table of directly-heard neighbours
  with link quality.
- **Multipoint Relay (MPR) selection** — each node selects the minimum subset of neighbours needed
  to reach all its two-hop neighbours. In a fully-connected cluster this set is empty and nothing
  relays.
- **Duplicate suppression** — origin ID + sequence number on every frame; a node that has already
  heard a frame does not relay it.
- **Passive acknowledgement** — a node queued to relay that overhears another node relay the same
  frame suppresses its own transmission.
- **Loop prevention** — hop count / TTL.

## Consequences

- The cluster case is free. When 12 leaders are within direct range of one another, every MPR set is
  empty, nothing relays, and the system behaves exactly like conventional simplex with no latency or
  capacity penalty. This is the required behaviour, and it falls out of MPR rather than needing a
  special case.
- Cost scales with dispersal, not headcount — the defining property of the product.
- Beacon overhead is a permanent tax on a 19.2 kbps channel. Standard OLSR HELLO at 2 s and TC at
  5 s was designed for links three orders of magnitude faster. Both the interval and the message
  encoding need to be re-derived for this channel, not inherited. See
  [OQ-0004](../open-questions.md).
- Because the medium is broadcast and voice is one-to-many, full link-state topology dissemination
  (OLSR's TC messages) may be unnecessary — MPR flooding alone may be sufficient. Confirming this
  would remove the more expensive half of OLSR's overhead. Phase 0 should test it.

## Reversal trigger

Phase 0 shows MPR selection failing to converge, or oscillating, with 12 nodes at walking pace and
realistic link-quality noise — particularly across partition and rejoin.

## Alternatives rejected

- **AODV / DSR / reactive.** Route discovery latency on every PTT press.
- **Plain flooding, no MPR.** Every node relays every frame. On a 4-slot channel with 12 nodes this
  saturates immediately, and destroys the cluster case that makes the product acceptable.
- **Batman-adv.** Designed for a shared broadcast medium with far more capacity, and its per-node
  originator message overhead is worse than OLSR's at this scale.
