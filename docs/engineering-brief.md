# Narrowband Voice MANET — Engineering Brief

**Project:** Multi-hop mesh voice radio for licensed UK VHF Business Radio spectrum
**Status:** Pre-development. Nothing built. This document defines the target.
**Audience:** Development agent / engineer starting from zero

---

## 1. What this is

A hand-portable two-way radio in which every handset is also a relay. Voice reaches its
destination by hopping through other handsets in the group. No repeater, no base station,
no infrastructure, no cellular, no internet.

It is a narrowband mobile ad-hoc network (MANET) carrying voice only, operating on licensed
UK Business Radio VHF spectrum.

This product does not exist. Every commercial MANET operates at 1.2–5 GHz with wideband
channels, wearable node form factors and external audio. Every narrowband PMR standard
(DMR, TETRA, dPMR, NXDN) uses fixed repeaters and has no multi-hop relay. This project sits
in the gap between them.

## 2. The operational problem

A youth organisation runs activities with approximately 12 leaders, three times a week plus
four residential trips a year, across arbitrary UK locations — hills, woodland, valleys,
moorland.

Two licensed fixed sites exist and cover the regular meeting point. Off-site, there is no
infrastructure and none can be licensed (Area Defined UHF assignments are unavailable;
deployable repeaters require site-specific coordination with ~42 working day lead times).

Off-site, leaders rely on simplex. The failure mode is:

- Front of group cannot hear back of group
- Splinter groups lose contact with each other and with the group leader
- Supervision and emergency escalation break down at exactly the moment they matter

Terrain, not distance, is the cause. A ridge or 200 m of woodland kills a link that would
work over open ground.

**Why a mesh solves it:** in a dispersed group there are almost always people between the
two who cannot hear each other. Those people become the path. Network capability improves
as the group spreads out, which is the opposite of every other technology available.

## 3. Hard requirements

These are non-negotiable and derived from operational reality, not preference.

| # | Requirement | Rationale |
|---|---|---|
| 1 | Belt-clipped brick with integrated speaker and microphone | Leaders light fires, pitch tents, cook, scramble with rucksacks. Cables and earpieces are snag hazards and get lost |
| 2 | No headset, no remote speaker mic, no worn node | Same reason. Also: a youth worker must not look tactical |
| 3 | Works with no infrastructure of any kind | No repeater can be licensed where they operate |
| 4 | No duty cycle restriction | Safety-related traffic spikes during incidents |
| 5 | Licensed spectrum with good propagation | Terrain penetration is the entire problem |
| 6 | All-day battery | Single-shift use, no charging opportunity |
| 7 | Audio no worse than DMR | Codec2 3200 has been listened to and accepted |
| 8 | Simple operation | Volunteers, shared kit, minimal training |

## 4. System design — decided

### 4.1 RF layer

| Parameter | Value | Notes |
|---|---|---|
| Band | VHF Mid Band 137.9625–165.04375 MHz or High Band 165.04375–173.09375 MHz | UK Business Radio. Both permit 25 kHz single-frequency channels |
| Channel width | 25 kHz | Chosen over 12.5 kHz to afford Codec2 3200 and four slots |
| Mode | Simplex (single frequency) | Same frequency TX and RX, TDD |
| Modulation | 4FSK | Scaling DMR's proven 9.6 kbps in 12.5 kHz |
| Gross bit rate | ~19.2 kbps | Assumption — based on DMR spectral efficiency. Verify against chosen transceiver |
| TX power | 5 W | IR 2044 permits 25 W ERP for mobile stations. 5 W chosen conservatively |
| Antenna | Helical, ~15–20 cm | Full quarter wave at 155 MHz is 48 cm — unwearable. Accept 3–6 dB loss |

### 4.2 MAC layer — TDMA

| Parameter | Value |
|---|---|
| Frame duration | 60 ms |
| Slots per frame | 4 |
| Slot duration | 15 ms |
| Per-slot rate | ~4.7 kbps (19.2 / 4) |
| Synchronisation | GPS-disciplined initially; network-derived is a later refinement |

The critical design feature is **slot pipelining**.

A relaying node receives in slot *n* and retransmits in slot *n+1* of the same frame. This
propagates a transmission three hops per 60 ms frame rather than one hop per frame.

- **Correct:** ~20 ms per hop average, 15 hops at 300 ms end-to-end
- **Wrong (wait for next frame):** 60 ms per hop, unusable beyond five

This single decision determines whether the system works. It is the reason IWAVE's
comparable product uses six slots rather than DMR's two.

Store-and-forward is explicitly rejected. It is simpler but produces unacceptable latency
and is not what "doing this properly" means.

### 4.3 Network layer — mesh routing

Derived from OLSR, adapted to a slotted narrowband half-duplex channel.

Required mechanisms:

- **Neighbour discovery.** Periodic beacons. Each node maintains a table of directly-heard
  neighbours with link quality. Beacon overhead must be small — this is a 19.2 kbps channel,
  not 19.2 Mbps.
- **Multipoint Relay (MPR) selection.** Each node selects the minimum subset of neighbours
  needed to reach all two-hop neighbours. In a fully-connected cluster this set is empty and
  no relaying occurs.
- **Duplicate suppression.** Every frame carries origin ID and sequence number. A node that
  has already heard a frame does not relay it.
- **Passive acknowledgement.** A node queued to relay that overhears another node relay the
  same frame suppresses its own transmission.
- **Loop prevention.** Hop count / TTL field.

### 4.4 Required behaviour

| Situation | Behaviour |
|---|---|
| 12 leaders clustered, all in direct range | Behaves exactly like conventional simplex. One transmission, everyone hears it, nothing relays, no latency or capacity penalty |
| Group strung out along a path | Relaying begins automatically through intermediate nodes |
| Splinter group beyond direct range | Routes through whoever is between |
| Nodes moving, links breaking and forming | Topology reconverges without user action |
| Single node alone | Works as a plain simplex radio |

Cost scales with dispersal, not with headcount. This is the defining property.

### 4.5 Voice layer

| Parameter | Value |
|---|---|
| Vocoder | Codec2 at 3200 bps — open source, no AMBE royalties |
| FEC + framing overhead | ~47%, giving ~4.7 kbps — same ratio DMR applies to AMBE+2 |
| Quality target | Equal to or better than DMR's AMBE+2 at 2450 bps |

Codec2 3200 has been evaluated against AMBE 2400 samples and judged acceptable.

## 5. Hardware platform

### 5.1 Recommended starting hardware

| Item | Qty | Purpose |
|---|---|---|
| TI CC1200 evaluation modules | 4 | Sub-1 GHz transceiver, 4FSK, narrowband 12.5/25 kHz support, good sensitivity, well documented |
| STM32F4-class dev boards | 4 | MCU. Codec2 runs on F4 — proven by the M17 project |
| Attenuators, dummy loads, SMA cables, splitter | — | Bench work with no radiation |
| TinySA Ultra or equivalent | 1 | Basic spectrum measurement |

### 5.2 Alternative transceiver

CML Microcircuits CMX7164 — multi-mode narrowband modem designed for custom protocols rather
than fixed standards. Likely the better production part. Engage CML early to establish exactly
what timing control is exposed.

### 5.3 Explicitly rejected

**SDR (ADALM-Pluto, LimeSDR, HackRF).** USB round-trip latency is milliseconds against a 15 ms
slot budget — slot discipline cannot be validated. Also the wrong instrument: a 20 MHz-capable
radio developing a 25 kHz waveform.

**Adapted DMR handhelds (MD-UV380, GD-77).** HR-C6000 baseband owns the framing and is locked
to DMR's two-slot structure.

**Motorola DP4800 and similar.** Signed, closed firmware. No baseband access.

## 6. Development phases

### Phase 0 — Simulation. No hardware, no cost.

Model the MAC and routing layers in software. Feed realistic mobility patterns.

Must answer:

- Does MPR selection converge with 12 nodes moving?
- Does the cluster case correctly produce zero relaying?
- Does slot pipelining resolve correctly across 3–5 hops?
- What is beacon overhead as a percentage of channel capacity?
- What happens when the network partitions and rejoins?

If the protocol fails here it will fail in hardware. Do not buy anything until this passes.

Suggested: ns-3, or a purpose-built simulator. A custom simulator may be faster than fighting
ns-3's abstractions for a bespoke slotted MAC.

### Phase 1 — Bench RF. Few hundred pounds. No licence required.

Radios wired through attenuators. Nothing radiates.

- CC1200 configured for 25 kHz 4FSK, verify achievable bit rate
- Implement TDMA framing and slot timing on STM32
- Verify slot alignment and guard intervals
- Integrate Codec2 3200, verify end-to-end audio over one link
- Then two hops, then three

### Phase 2 — Over the air.

Requires an Ofcom Innovation and Trial licence — the mechanism for testing equipment that does
not yet meet standard requirements. Time-limited, area-defined.

- Range measurement, real terrain
- Multi-hop with real geometry
- Mobility and reconvergence

### Phase 3 — Productisation.

Custom PCB, PA to 5 W, enclosure, battery, audio path, conformity testing.

## 7. Regulatory constraints

**Building and bench testing is unregulated.** No permission needed to design, build, write
firmware, or test into dummy loads.

**Over-the-air transmission requires authorisation.** Wireless Telegraphy Act 2006 s.8.
Innovation and Trial licence is the route during development.

**Operational use requires conformity to IR 2044**, which references EN 300 113-2,
EN 301 166-2 and related harmonised standards.

Self-assessment (Module A) is available provided harmonised standards are applied in full. No
Approved Body required. Critically: those standards test RF parameters — frequency error,
adjacent channel power, spurious emissions, sensitivity, blocking — not protocol behaviour. A
mesh MAC does not push the product out of self-declaration.

Watch adjacent channel power during burst transitions. TDMA transmitter attack and release
times are where a fast, dirty slot switch fails EN 300 113. Design to it from the start.

Four assessments needed for full compliance:

| RED Article | Covers | Standard |
|---|---|---|
| 3.2 | Efficient spectrum use | EN 300 113-2 / EN 301 166-2 |
| 3.1(b) | EMC | EN 301 489-1 and -5 |
| 3.1(a) | Electrical safety | EN 62368-1 |
| 3.1(a) | RF exposure | EN 62479 / EN 50665 |

**On licensing the finished system:** Ofcom's own definition of a base station is "a fixed
location at which transceivers and antennas are installed". A carried handheld is not a fixed
location. The TFAC (OfW164) contains no reference to repeating, relaying, talk-through or
retransmission — it is silent, neither permitting nor prohibiting. There is an existing
mobile-only, single-frequency assignment class called Operational Area, defined as "areas with
a defined radius over which mobile to mobile communication is allowed in the absence of a base
station."

## 8. Open decisions

1. Achievable gross bit rate at 25 kHz with CC1200. The 19.2 kbps figure is scaled from DMR and
   must be verified. Slot count and vocoder rate both depend on it.
2. Synchronisation method. GPS-disciplined is simplest. Network-derived is more elegant, works
   indoors and under canopy, and is considerably harder.
3. Beacon interval. Trade-off between reconvergence speed and channel overhead.
4. Slot count. Four is derived from Codec2 3200 plus FEC at 19.2 kbps. If the real bit rate
   differs, revisit.
5. VHF Mid Band or High Band. Propagation is near-identical; availability of a 25 kHz simplex
   assignment decides it.
6. Encryption. Not required, but AES is cheap to add and expected in this class of product.
7. In-house or contracted development.

## 9. Success criteria

**Phase 0:** protocol converges in simulation with 12 mobile nodes; cluster case produces zero
relaying; 5-hop chain resolves within 300 ms.

**Phase 1:** intelligible Codec2 3200 voice across three hops on the bench, with measured slot
timing within guard intervals.

**Phase 2:** two dispersed groups in real terrain, out of direct range of each other, holding a
conversation through intermediate handsets, with no user action.

**Ultimate:** a leader at the back of a dispersed group on a hillside can talk to a leader at
the front, and neither of them knows or cares that four other radios carried it.

## 10. Things that are not in scope

- Data, video, telemetry, situational awareness. Voice only.
- Interoperability with DMR, TETRA or any existing standard.
- Infrastructure of any kind — no repeaters, base stations or gateways.
- Cellular, satellite or internet backhaul.
- Trunking, registration, or centralised control of any sort.

## Appendix — why the obvious alternatives were eliminated

| Option | Why not |
|---|---|
| DMR / TETRA / P25 / NXDN | No multi-hop. All require fixed infrastructure |
| MOTOTRBO ERDM | Single hop only, and the repeater is still a licensed base station |
| Commercial MANET (Silvus, Persistent, DTC, TrellisWare) | 1.2–5 GHz. Poor terrain propagation, worn node form factor, external audio, £10k/node |
| Doodle Labs Mesh Rider | Right concept, but 900 MHz variant is US ISM and unavailable here; UK-legal option is 2.4 GHz at 100 mW |
| REGULUS MU7 | 868 MHz — UK duty cycle limits (1–10%) make it unusable for safety-related voice |
| IWAVE Defensor-T4 | Technically the closest match — 400–470 MHz, 12.5 kHz, 6-slot mesh, belt brick. Rejected on supply chain grounds |
| goTenna Pro X2 | Right bands, right topology, proven at 15 miles body-worn — but carries no voice |
| DECT NR+ | Standardised mesh, but 1.9 GHz at 250 mW. Roughly 25 dB down on VHF at 5 W; insufficient node density to chain short hops |
| PoC / WAVE PTX | Server-mediated, no direct mode, no off-network capability |
| Satellite (Starlink, Iridium) | Solves reach-back to base, does nothing for leader-to-leader in a dispersed group |
