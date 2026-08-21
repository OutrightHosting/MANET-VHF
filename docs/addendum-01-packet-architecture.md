# Addendum 01 — Packet Architecture and Gateway Nodes

**Applies to:** [Narrowband Voice MANET — Engineering Brief](engineering-brief.md)
**Status:** Architectural constraint. Apply from the first line of code.
**Read before:** writing any MAC, framing or routing code.

---

## Why this addendum exists

The main brief specifies a voice system. Read literally, that invites a design where voice is the
payload and everything else is bolted on afterwards.

Don't build it that way. **Build a packet network that happens to carry voice as its primary traffic
type.**

This is not a request for extra features now. It is a constraint on how the frame format and
dispatch logic are structured, so that later additions do not require rewriting the MAC.

Nothing else in the brief changes. RF layer, 25 kHz, 4FSK, 4-slot TDMA, 60 ms frame, slot
pipelining, MPR routing, Codec2 3200 — all unchanged.

## 1. The core constraint

**Every transmission is an addressed, typed frame. Voice is one frame type among several.**

Do not write a voice streaming protocol with signalling bolted on. Write a frame-switched network
with a voice payload type.

## 2. Frame header — minimum required fields

Design the header now even if only voice is implemented.

| Field | Purpose | Notes |
|---|---|---|
| Source address | Originating node | See §3 |
| Destination address | Target node, group, or broadcast | Must support group addressing from the start |
| Frame type | Payload discriminator | Reserve generous space. Cheap now, expensive later |
| Sequence number | Duplicate suppression | Already required by the routing design |
| TTL / hop count | Loop prevention | Already required |
| Priority | QoS class | See §5 |

Header bytes are expensive at 4.7 kbps per slot. Be frugal — but be frugal about *field width*, not
about *which fields exist*. Omitting a type field to save four bits will cost a rewrite.

## 3. Addressing

Define an addressing scheme covering:

- **Individual node** — unique per radio
- **Group / talkgroup** — multiple nodes, the normal case for PTT
- **Broadcast** — all nodes, used by neighbour discovery and beacons
- **Reserved range** — for gateways and future node classes

Do not assume every node is a handheld. See §4.

## 4. Node types

The network has at least two classes, and the protocol must not assume otherwise.

**Handheld node.** Mobile, battery, integrated audio, participates in routing and relays for others.
The normal case.

**Gateway node.** Participates in the mesh exactly like a handheld — same MAC, same routing, same
slot discipline — but has a wired interface (Ethernet or USB) instead of a user. Bridges mesh
traffic to external systems.

The routing layer should treat a gateway as an ordinary node. The only difference is what it does
with frames addressed to it, and what it originates.

**Regulatory note:** a gateway is a fixed installation with an antenna, which makes it a base
station under Ofcom's definition. That is acceptable at a licensed site and is why the system
targets a Technically Assigned licence rather than Simple UK. It has no bearing on the protocol
design — but it means gateways must be assumed to be stationary and mains-powered, which permits
design choices unavailable to handhelds (higher power, better antenna, always-on, no battery
constraint, potentially a stable time reference for network sync).

## 5. Priority and queueing

Implement a priority field and honour it from the outset.

| Class | Traffic | Behaviour |
|---|---|---|
| 0 | Emergency | Pre-empts everything |
| 1 | Voice | Pre-empts data. Never queued behind bulk traffic |
| 2 | Signalling / routing | Beacons, topology updates |
| 3 | Data | Best-effort, yields to all above |

Voice must never be delayed by a text message or a position report. On a 4-slot channel this matters
more than it would on a fat pipe.

## 6. What this enables later

None of these need building now. They should all be possible **without touching the MAC or routing
layers.**

| Application | Mechanism |
|---|---|
| Dispatch console | PC on a gateway node |
| Telephone interconnect | SIP endpoint on a gateway, transcoding Codec2 ↔ G.711 |
| Text messaging | New frame type, priority 3 |
| Location reporting | GPS is already present for slot sync — expose it as a frame type |
| Voice logging | Gateway node that records everything it hears |
| Man-down / emergency alert | Frame type at priority 0 |
| Remote configuration | Frame type, priority 3 |

If adding any of these later requires changing the frame header or the routing logic, the
architecture was wrong.

## 7. Capacity discipline

Four slots at roughly 4.7 kbps. This is a narrowband voice network with a small data capability, not
a data network that also does voice.

- Voice always wins
- Data is best-effort and must be rate-limited
- Position reports and text are cheap; anything bulk is out of scope
- No video, no file transfer, no telemetry streaming. If a proposed feature needs sustained
  throughput, it does not belong on this system

## 8. What to do with this before writing code

1. Design the frame header including all fields in §2, even where unused.
2. Define the addressing scheme in §3, including reserved ranges.
3. Define the frame type enumeration with reserved values.
4. Structure the receive path as a dispatcher on frame type, not as a voice decoder with exceptions.
5. Implement the priority queue in §5 even if only voice and signalling exist initially.
6. Ensure the routing layer has no knowledge of payload type.

That is the whole of it. Roughly a day of design work that prevents a rewrite in six months.
