# Open questions

Live register of undecided items. Each has an owner phase — the earliest phase at which it can
actually be answered — and a statement of what it blocks. When one closes, it becomes an ADR (or an
amendment to an existing one) and is struck through here rather than deleted.

OQ-0001 and OQ-0003 through OQ-0008 come from [engineering brief §8](engineering-brief.md#8-open-decisions).
OQ-0012 comes from [Addendum 01](addendum-01-packet-architecture.md). OQ-0014 through OQ-0016 come
from [the feature set](feature-set.md). The rest surfaced while writing the decision log and are not
yet in any of them.

| # | Question | Answerable in | Blocks | Status |
|---|---|---|---|---|
| [OQ-0001](#oq-0001) | Achievable gross bit rate at 25 kHz on CC1200 | Phase 1 | **THE critical path** — with the vocoder, decides FEC | **Open, blocking** |
| [OQ-0024](#oq-0024) | Acquisition preamble — 56 bit-times, or 128 if the LO free-runs | Phase 1 | Reach. 3 hops vs 12 | **Open, blocking** |
| [OQ-0025](#oq-0025) | PA chain from the CC1200's +16 dBm to 5 W | Phase 2 | Range. The sim assumes 37 dBm and the modem gives 16 | Open |
| [OQ-0026](#oq-0026) | TX duty cycle — measured 20% worst node, vs a handheld's 5% | Phase 2 | Thermal, battery, **and who dies first** | Open |
| [OQ-0027](#oq-0027) | Which VHF band the transceiver is actually characterised for | Before the RF design | Whether the part is specified in our band or not | Open |
| [OQ-0028](#oq-0028) | Do two co-slot relays carrying an identical payload decode? | Phase 1 | **Every delivery figure in the project.** 7 hops vs 3 | **Open, blocking** |
| [OQ-0029](#oq-0029) | FFI deferred automatic relaying as possibly infeasible in 25 kHz | Phase 0 | Whether the core premise holds | **Open — risk register** |
| [OQ-0030](#oq-0030) | Are we optimising the wrong thing — hop count instead of range per hop? | **Now, it is a strategy question** | Direction of the whole MAC effort | **Open** |
| [OQ-0031](#oq-0031) | GPS holdover and network time transfer | Phase 0 design, Phase 1 measure | Whether losing GPS loses the network | **Open** |
| [OQ-0032](#oq-0032) | Dense cover spends the hop budget before the group ends | **Phase 0** | Coverage in woodland. 12/12 connected, 8/12 hearing | **Open** |
| [OQ-0033](#oq-0033) | ~~Half of all payloads die at the talker's first hop~~ | — | — | **Closed** — simulator counted undetectable signals as interference |
| [OQ-0034](#oq-0034) | ~~Relay decision refused unknown senders~~ | — | — | **Closed** — an unknown sender now relays; see backlog B-13 |
| [OQ-0002](#oq-0002) | The slot budget does not close — structurally, once itemised | **Phase 0** | Everything downstream of the MAC | Open |
| [OQ-0003](#oq-0003) | Synchronisation: GPS-disciplined or network-derived | Phase 0 (design), Phase 2 (proof) | Guard interval size, canopy/indoor operation | Open |
| [OQ-0004](#oq-0004) | Beacon interval, and where control traffic lives in the slot structure | **Phase 0** | Channel overhead, reconvergence time | Open |
| [OQ-0005](#oq-0005) | ~~Slot count — is 4 right?~~ | — | — | **Closed** — [ADR-0008](decisions/0008-four-slots.md), 4 is forced |
| [OQ-0006](#oq-0006) | ~~VHF Mid Band or High Band~~ | — | — | **Closed** — superseded by [OQ-0027](#oq-0027), which answers it on silicon |
| [OQ-0007](#oq-0007) | Encryption | Phase 3 | BOM, key management UX | Open |
| [OQ-0008](#oq-0008) | In-house or contracted development | Commercial | Schedule and cost | Open |
| [OQ-0009](#oq-0009) | Channel access — beacons defer, relays are receiver-decided and staggered | Phase 1 | Simultaneous-PTT arbitration still unspecified | Open — no longer blocking |
| [OQ-0010](#oq-0010) | RX→TX turnaround budget on a half-duplex transceiver | Phase 1 | Guard interval, therefore payload, therefore OQ-0002 | Open |
| [OQ-0011](#oq-0011) | ~~Is full OLSR topology-control dissemination needed at all?~~ | — | — | **Closed** — TC stays |
| [OQ-0012](#oq-0012) | Header field widths and total header size | **Phase 0** | ~~OQ-0002~~; the header format is forever | Open — no longer blocks OQ-0002 |
| [OQ-0013](#oq-0013) | ~~Spatial reuse distance and its coupling to slot count~~ | — | — | **Answered** — 3 slots unsafe |
| [OQ-0014](#oq-0014) | Authenticating command frames on an infrastructure-free mesh | Phase 3 (design now) | Whether radio disable can safely exist at all | Open |
| [OQ-0015](#oq-0015) | Late entry — joining a call already in progress | **Phase 0** | Header/framing; costs bits that do not exist | Open |
| [OQ-0016](#oq-0016) | Confirmed transactions across a moving multi-hop path | Phase 0 | Every confirmed feature in the set | Open |
| [OQ-0017](#oq-0017) | Concurrent call capacity, clustered vs chained | **Phase 0** | Feature parity with DMR; the 3-vs-4 slot decision | Open |
| [OQ-0018](#oq-0018) | ~~The header cannot express the previous hop~~ | — | — | **Closed** — field added |
| [OQ-0019](#oq-0019) | ~~Uniform propagation cannot express a blocked link~~ | — | — | **Closed** — [ADR-0010](decisions/0010-terrain-diffraction.md), model built |
| [OQ-0020](#oq-0020) | How large can the network be? | **Phase 0** | Sizing of TTL, tables and beacon pool | Open — earlier answer was wrong |
| [OQ-0021](#oq-0021) | Many-to-many — streams now cross; `dst` still inert | **Phase 0** | Addressed calls; concurrent-stream quality | Open — no longer total failure |
| [OQ-0022](#oq-0022) | ~~Which latency budget applies~~ | — | — | **Closed** — 500 ms mouth-to-ear |
| [OQ-0023](#oq-0023) | ~~Vegetation model~~; ~~exponent challenged by FFI's γ=4~~ | — | — | **Closed** — M-01: our model agrees with Egli within 11%; the real uncertainty is 2× and lives in the link budget |

---

## OQ-0001
### Achievable gross bit rate at 25 kHz with the CC1200

The 19.2 kbps figure is scaled from DMR's proven 9.6 kbps in 12.5 kHz — doubling the channel and
assuming equivalent spectral efficiency at 4FSK. It is an assumption, not a measurement, and
[ADR-0002](decisions/0002-tdma-slot-pipelining.md), [ADR-0004](decisions/0004-codec2-3200.md),
[ADR-0007](decisions/0007-packet-switched-frame-architecture.md) and the entire slot structure sit
on top of it.

Cannot be closed before hardware. Until then, Phase 0 should treat gross rate as a **parameter**,
not a constant, and report results across a range — at minimum 16.0, 19.2 and 22.4 kbps — so that
whatever the CC1200 turns out to deliver, the answer is already on the shelf.

## OQ-0002
### The slot budget does not close — structurally, once itemised

**The most consequential open item in the project**, and materially worse than it first appeared.

The brief's "~47% FEC and framing" is a blanket figure inherited from DMR.
[Addendum 01](addendum-01-packet-architecture.md) forces it to be itemised — the header is now a
named, sized object rather than part of a lump — and itemising it is where the problem shows. At
4 slots × 15 ms × 19.2 kbps:

| | Bits |
|---|---|
| Raw bits per slot | 288 |
| Less guard at DMR-equivalent proportion (2.5 ms in 30 ms = 8.3%) | −24 |
| **On-air bits** | **264** |
| Less sync / preamble (24 assumed; DMR spends 48) | −24 |
| Less header ([ADR-0007](decisions/0007-packet-switched-frame-architecture.md), 34 bits) | −34 |
| Less Codec2 3200 per 60 ms frame | −192 |
| **Left for FEC** | **14** |

Fourteen bits is a CRC, not forward error correction. On a narrowband channel with no
retransmission, carrying voice that must stay intelligible at the fringe of range, that is not a
viable frame.

Guard cannot simply be trimmed. It has to cover transmitter attack and release — which
[EN 300 113 tests directly, via adjacent channel power during burst transitions](engineering-brief.md#7-regulatory-constraints)
— plus RX→TX turnaround ([OQ-0010](#oq-0010)) and sync uncertainty ([OQ-0003](#oq-0003)).
Propagation delay is negligible: 50 µs at 15 km.

### The four escape routes, re-costed

| # | Route | Assessment |
|---|---|---|
| 1 | **Raise gross rate** to ~22 kbps | Depends entirely on [OQ-0001](#oq-0001) and is not in our gift. Cannot be planned on |
| 2 | **Cut FEC and framing** | **Eliminated.** Itemisation shows FEC is what has already been squeezed to nothing; and a zero-length header still leaves only 25% — see findings below |
| 3 | ~~**3 slots × 20 ms**~~ | **ELIMINATED by [OQ-0013](#oq-0013).** Spatial reuse fails in open terrain: 9.6 dB C/I against a 10 dB capture requirement, 34% delivery. Was the leading route; is now unusable. Formerly: Leaves 102 bits for FEC — a 45% ratio, on the brief's own target. Now known to cost three separate things: a hop of spatial reuse margin ([OQ-0013](#oq-0013)), a concurrent call when clustered ([OQ-0017](#oq-0017)), and a third more beacon airtime ([OQ-0004](#oq-0004)) |
| 4 | **120 ms voice superframe** | Still viable. One codec payload spans two slot opportunities; preserves 4 slots and the gross rate, at the cost of latency and framing complexity. Also halves per-payload header overhead, which is worth something |
| 5 | **Drop to Codec2 2400** | Newly worth listing. Frees 48 bits and directly relieves the deficit, at the cost of hard requirement 7 becoming a much closer call. Prefer 3 and 4 first |

Two things worth noting before route 3 is adopted by default. The latency argument against it is
weaker than the brief implies — under pipelining a payload advances one hop per *slot*, so a 5-hop
chain costs 100 ms at 3×20 ms against a 300 ms criterion. And its real cost, spatial reuse margin,
is [OQ-0013](#oq-0013) and is not yet characterised.

### Phase 0 findings — 2026-08-21

The budget is no longer arithmetic in a document. `core/include/manet/config.h` computes it from
compile-time parameters and enforces it by static assertion, and `make budget` sweeps candidate
frame structures by actually compiling each one. The straw-man reproduces exactly: 288 raw, 264 on
air, 34 header, **14 bits for FEC**.

| Configuration | On air | Header | FEC | Ratio |
|---|---|---|---|---|
| 4×15 ms, 19.2k (ADR-0007 straw-man) | 264 | 34 | 14 | 6% |
| 3×20 ms, 19.2k | 352 | 34 | 102 | **45%** |
| 4×15 ms, 22.4k | 308 | 34 | 58 | 25% |
| 4×15 ms, 16.0k | 220 | 34 | −30 | infeasible |
| 3×20 ms, 16.0k | 293 | 34 | 43 | 19% |
| 4×15 ms, 19.2k, Codec2 2400 | 264 | 34 | 62 | 34% |

**The decisive result: at 4×15 ms / 19.2 kbps / Codec2 3200, a header of length *zero* would still
leave only 48 bits — a 25% ratio against DMR's 47%.** The 4-slot structure cannot reach
DMR-equivalent protection under any header design, so no amount of header frugality rescues it.
This eliminates route 2 completely and confirms the problem is structural: slot length, bit rate, or
vocoder. Nothing else moves it.

Route 3 (3×20 ms) is the only configuration in the sweep that both closes and holds at 19.2 kbps
without touching the vocoder. But three separate costs have since attached to it —
[OQ-0013](#oq-0013), [OQ-0017](#oq-0017) and [OQ-0004](#oq-0004) — and route 4, the 120 ms
superframe, pays none of them. Route 4 has not been modelled and should be, before route 3 is
adopted by default.

Phase 0 should still evaluate routes 3, 4 and 5 against simulated behaviour rather than adopting
route 3 on budget grounds alone — the budget says which frames can exist, not which ones work.

## OQ-0003
### Synchronisation method

GPS-disciplined is simplest and is the Phase 0/1 assumption. Network-derived sync is more elegant,
works under canopy and indoors — both real operating conditions for this group, not edge cases — and
is considerably harder.

Bears directly on [OQ-0002](#oq-0002): GPS gives sub-microsecond alignment and a small sync
allowance within the guard interval. Network-derived sync is looser and costs guard time, which
costs payload.

[Addendum 01 §4](addendum-01-packet-architecture.md#4-node-types) raises gateways as a possible
stable time reference — mains-powered, stationary, no battery constraint. Genuinely useful where a
gateway exists, but it does not answer this question: a gateway needs a licensed fixed site, and the
operational problem is off-site, where by definition there is none. Treat it as an optimisation at
the regular meeting point, not a route out of GPS.

Decide the Phase 0/1 approach now (GPS), but do not design the frame structure in a way that makes
network-derived sync impossible to retrofit.

## OQ-0004
### Beacon interval, and where control traffic lives in the slot structure

Two questions that have been treated as one, and are not:

**Interval** is the stated trade-off between reconvergence speed and channel overhead. Standard OLSR
HELLO at 2 s and TC at 5 s was designed for links three orders of magnitude faster than this one and
should not be inherited. Phase 0 should sweep the interval and report reconvergence time against
overhead as a percentage of channel capacity — one of Phase 0's five required answers.

Note that [OQ-0011](#oq-0011) closing against dropping TC means this overhead is now unavoidable,
which raises the stakes on getting the interval right.

**Placement is not specified anywhere in the brief.** Beacons and topology control contend with
voice for the same slots. [Addendum 01](addendum-01-packet-architecture.md) gives them a priority
class (2) and frame types (`0x2`, `0x3`), which says what happens when they compete with voice — but
not where they physically sit in the slot structure. The options — a dedicated control slot, stealing
an idle voice slot, piggybacking on voice frames, or a periodic control superframe — have materially
different costs, and the choice interacts with [OQ-0005](#oq-0005) and [OQ-0009](#oq-0009).

Interacts with [OQ-0002](#oq-0002) in an uncomfortable direction: a dedicated control slot is the
cleanest answer and the one the payload budget can least afford.

### Phase 0 findings — 2026-08-21

With `neighbour` built, the beacon is a real object and its cost is computable. From
`make budget`, for 12 leaders at the inherited 2 s interval:

| | 4 × 15 ms | 3 × 20 ms |
|---|---|---|
| One beacon | 148 bits | 148 bits |
| What beacons **say**, as a share of channel capacity | 4.6% | 4.6% |
| What beacons **occupy**, as a share of slots | **9.0%** | **12.1%** |
| Wasted to packing, per beacon | 92 bits | 180 bits |

**The gap between saying and occupying is the whole problem.** A beacon is 148 bits; a slot is 240
or 328. A slot is the smallest thing that can be transmitted, so the remainder is thrown away. The
information cost is comfortable; the airtime cost is three times worse and comes straight out of
voice.

**Beacon airtime is linear in group size, and this is the ceiling that cannot be engineered
away.** Every radio must announce itself once per interval, and a beacon needs a whole slot:

| Radios | Slots per interval | Share of ALL airtime |
|---|---|---|
| 12 | 12 of 132 | 9.1% |
| 24 | 24 of 132 | 18.2% |
| 48 | 48 of 132 | 36.4% |
| 100 | 100 of 132 | **75.8%** |

Table size and TTL can both be raised — one is memory, the other two header bits. This one is
channel capacity, and at around forty radios beacons are eating a third of everything.

**And the 3-slot structure is worse at it**, because its slots are larger and therefore waste more —
12.1% against 9.0%. This is a **third cost of the leading escape route from
[OQ-0002](#oq-0002)**, alongside the reuse-distance cost in [OQ-0013](#oq-0013) and the concurrent
call cost in [OQ-0017](#oq-0017). None of the three is individually decisive. Together they mean
3 × 20 ms should not be adopted on budget arithmetic alone.

Three ways to close the packing gap, none designed: pack several beacons into one slot;
piggyback beacon data on voice frames; or lengthen the interval and accept slower reconvergence.
The interval itself is still inherited from OLSR rather than derived — see above.

### Phase 0 findings, part two — 2026-08-21. Beacon *phase* is a hard requirement.

Chasing an unexplained 79% end-to-end delivery produced the sharpest result so far, and it
is not about how often beacons are sent but *when*.

A relay must listen in the slot its upstream neighbour transmits in. A half-duplex radio
keying up to send a beacon in that slot is **deaf**, so the payload dies at that hop and
every node downstream loses it too. Losses compound along the chain.

The harness staggered beacons by `interval / node_count`, which at the default interval
gives a step of 16 slots. Sixteen is a multiple of the four slots per frame — so *every*
node's beacon landed on the same phase of the voice cycle, which is precisely the phase the
originator transmits in. Every relay went deaf in exactly the slot that mattered.

Fixed by stepping the stagger with a value coprime to the frame length. End-to-end delivery
in an 8-node chain went from **78.8% to 99.8%**, and collisions from 42 to zero. Forcing the
bad alignment back reproduces the old figures exactly.

**This is a design requirement the brief and the addendum do not state.** Where control
traffic sits in the slot structure is not merely a capacity question — get the phase wrong
and voice relaying degrades in a way that looks like poor coverage and would be extremely
hard to diagnose on a hillside. Any answer to this question must specify beacon *phase*
relative to the voice cycle, not just interval and placement.

A second defect found in the same investigation, and worth recording because it very nearly
became a false finding: the delivery metric keyed on `(source, sequence)`. The sequence
number is 8 bits and wraps every 256 payloads — **15.4 seconds of continuous talking** — so
long runs silently merged distinct payloads and reported the loss as a delivery failure. The
wire format is fine; duplicate suppression ages entries out long before a wrap. The metric
was wrong. It now attributes each receipt to the most recent origination at or before it.

That 15.4-second figure is itself worth carrying into [OQ-0012](#oq-0012): it is the hard
limit on how long duplicate-suppression state may live, and it is not generous.

## OQ-0005
### ~~Slot count~~

**Closed** by [ADR-0008](decisions/0008-four-slots.md), 2026-08-21. **Four, and it is forced
rather than chosen:** spatial reuse requires at least four ([OQ-0013](#oq-0013)) and the voice
payload fits in at most four — at five slots a 12 ms slot carries 211 bits and sync plus header
plus Codec2 3200 need 258. Only one value satisfies both.

Original reasoning, retained:

The coupling is now better understood than when this was first written, and it is not the one the
brief describes. Slot count sets:

- **per-slot payload** — fewer, longer slots carry more bits each, which is what makes 3×20 ms the
  strongest escape from OQ-0002;
- **spatial reuse distance** — the real cost of fewer slots, see [OQ-0013](#oq-0013);
- **hop latency** — equal to slot duration, so *more* slots is faster per hop, not slower. 15 ms at
  4 slots, 20 ms at 3. Both pass the 300 ms criterion at 5 hops with large margin.

The brief's framing of this as "hops per frame" is not the right axis.

## OQ-0006
### ~~VHF Mid Band or High Band~~ — closed

Propagation is near-identical between the two, so this was never an engineering question.
Superseded by [OQ-0027](#oq-0027), which answers it from the transceiver datasheet instead:
High Band sits inside the part's characterised range and Mid Band does not.

Spectrum availability and licensing are out of scope for this repository.

## OQ-0007
### Encryption

Not required. AES is cheap to add and is expected in this class of product. The real cost is not the
cipher, it is key management on kit that is shared between volunteers with minimal training — which
collides with hard requirement 8. Defer to Phase 3.

**Note that this is confidentiality, and confidentiality is not the pressing half.** Authentication —
proving a frame came from the node it claims — is a separate problem with a harder deadline, because
[the feature set](feature-set.md) includes commands that act on a radio rather than merely being
heard by it. Tracked separately as [OQ-0014](#oq-0014). Solving confidentiality does not solve it,
though a shared key does most of the work for both.

[ADR-0007](decisions/0007-packet-switched-frame-architecture.md) reserves frame type values
`0x8`–`0xF` and address range `0xF0`–`0xFE`, which is where any key-management traffic would live.
Confirm before Phase 3 that no header bit is needed for a cipher/plaintext discriminator — if one is,
it must be found now, not after radios ship.

## OQ-0008
### In-house or contracted development

Commercial, not technical. Recorded here for completeness.

## OQ-0009
### Channel access: who owns a slot, across four priority classes

**A gap in the specification, not just an undecided parameter**, and Addendum 01 widened it.

The brief defines a TDMA frame and a pipelining rule, but never says how a node acquires the right
to originate in a given slot. Unanswered: which slot does an originator start in? If every
originator starts in slot 0, what happens when two leaders press PTT in the same frame? Is there a
contention or reservation mechanism, or is it first-heard-wins with the loser's audio lost? How does
a node learn a transmission is in progress and defer?

Addendum 01 adds a second half. There are now four priority classes contending for the same slots,
and the addendum states the policy — emergency pre-empts everything, voice never queues behind data,
data is best-effort and rate-limited — without a mechanism to implement it. Specifically:

- How does a priority-3 data frame ever acquire a slot without displacing voice or breaking a
  pipelined relay chain mid-stream?
- What does "pre-empts" mean mid-transmission for an emergency frame — does it interrupt an
  in-flight voice stream, or take the next available slot?
- Rate limiting is mandated but unspecified: per-node, per-network, token bucket, or a hard cap on
  data slots per superframe?

Conventional simplex resolves the PTT half socially — you hear that someone is talking and you wait.
That may well be the right answer, and it fits requirement 8. But it needs to be a decision with a
mechanism behind it, and it does not resolve the priority half at all.

### Phase 0 findings — 2026-08-21. This is now the blocking gap, not an open parameter.

**There is no channel access mechanism at all, and the design does not work without one.**
Slots are not assigned to anybody. Every radio transmits whenever its own scheduler says so,
and nothing prevents two radios keying up in the same slot. The priority rules in
[Addendum 01 §5](addendum-01-packet-architecture.md#5-priority-and-queueing) arbitrate
*within* one radio's scheduler and say nothing about arbitration *between* radios.

Voice alone is fine: a static 8-node chain delivers 99.8% with zero collisions. Beacons alone
are fine. Together they destroy each other, and the failure is not graceful.

**Clustered, twelve leaders in direct range.** Beacons and voice collide in the air because
there is no spatial reuse to separate them — everyone hears everyone. Three of twelve radios
had their beacon land on the voice slot phase *every interval*, so they were never heard,
neighbour tables stayed incomplete, phantom two-hop neighbours appeared, and relays were
selected in a group where nothing should relay at all.

Mitigated by a rule this register did not previously contain: **a radio must not beacon in a
slot phase it needs for voice** — neither the phase it transmits in nor the phase it must
listen in. With that, the cluster case passes: zero relays selected, zero relay
transmissions, delivery above 99%.

**Dispersed and moving, the mitigation is not enough.** As the group stretches to 3 km the
voice stream pipelines through *every* slot phase somewhere along the chain, so there is no
free phase to retreat to. Beacon reception per transmission falls from 11 neighbours to 1.0,
collisions run into the hundreds per 20 s, and the relay gate — a node's knowledge that its
upstream neighbour has selected it — is open on **0 to 4 of 11 hops** at any moment. The
control plane starves, the relay chain never forms, and end-to-end delivery becomes erratic
between 4% and 100% with no relationship to how far apart people are.

Topology *converges* throughout (100% of samples) and every node remains reachable. The
routing is not wrong. It simply cannot get its own control traffic delivered.

### Phase 0 findings, part three — a dedicated control slot, tried and measured

The obvious answer is the one TDMA exists to provide: reserve a slot that voice may never
use. It was tried. The result is instructive and the design is not yet right.

**First, a claim in this register was wrong.** It said a chain carrying continuous voice
saturates the channel, so there was no airtime to reserve from. Measured: an eleven-hop
chain under continuous voice occupies **51% of slots**. Nearly half the channel is idle
even at full load. There is plenty to reserve.

**But a fixed periodic reservation makes things worse, not better.** One slot in eight —
12.5% of airtime that voice was not using — dropped worst-case delivery under movement
from 77% to 60%. The reserved slot punches a hole in the relay pipeline: the payload that
needed it is delayed a slot, the delay cascades down the chain, and it collides with the
payload behind. The spare airtime is real but it is *structured by the pipeline*, not
uniformly available for the taking.

| Reserved | Worst | Mean |
|---|---|---|
| none | 77.4% | 88.0% |
| 1 in 8 (12.5%) | 59.9% | 79.7% |
| 1 in 16 (6%) | 67.6% | 83.9% |
| 1 in 32 (3%) | 81.8% | 87.4% |
| 1 in 64 (2%) | 89.8% | 94.5% |

The 1-in-64 row looks like the answer and is not. At that spacing a 132-slot beacon
interval contains only **two** control slots, so twelve radios crowd onto two and their
beacons collide wholesale. The apparent gain came from breaking beaconing, and everything
else broke with it: the clustered case relayed 16,523 times when it should relay none,
partition collapsed to 2 of 12 reachable, and the static chain fell from 97% to 64%.
Reverted.

**So the requirement is now precise, and the two halves conflict at present sizing:**

1. Control slots per beacon interval must be **at least the number of radios in earshot**,
   or beacons collide.
2. Reserved slots must be **sparse enough not to disrupt the pipeline** — 1 in 32 or
   rarer, on the evidence above.

At twelve radios and a 132-slot interval those cannot both hold: 1-in-32 gives four slots
for twelve radios. They reconcile by **lengthening the beacon interval** so that sparse
reservations still yield one slot per radio — 1 in 32 over a 384-slot interval gives twelve.
That trades reconvergence speed for airtime and is untested.

### Resolved for the voice path — 2026-08-21. Phase 0 gate passes 5/5.

Worst-node delivery under movement went from 61.5% to **96.8%**, mean to 98.6%, with the
clustered case still relaying nothing and partition recovering fully in 6 s.

Four changes, in the order they mattered:

1. **Beacons defer to voice.** Listen-before-speak, bounded so a radio cannot let its
   neighbours age it out mid-call. Fixed the clustered case outright.
2. **The relay decision moved from sender-told to receiver-computed.** Being *told* you are
   a relay requires a beacon, and beacons are exactly what cannot get through while someone
   is talking — so a radio that missed one did not relay, and one closed gate blacked out
   the whole chain downstream.
3. **Candidate relays staggered by link quality.** Receiver-decided relaying has a weakness
   sender-decided MPR does not: every candidate reaches the same conclusion in the same slot
   and they all fire together, so passive acknowledgement cannot help. Ranking on link
   quality means the strongest goes first and the others cancel — and if it fails, the next
   covers a slot later, which sender-decided selection cannot do at all.
4. **A radio at the frontier relays even when pruning says it needn't.** This was the one
   that mattered. Pruning alone stops the network dead where radios are packed closely:
   every candidate correctly concludes it adds no coverage, so nobody relays. Measured as a
   chain delivering 100% for six hops and 2% at the seventh, gate closed at a radio 168 m
   from its upstream in 528 m of range. A radio can tell it is at the edge of the sender's
   reach from its own link quality alone, without knowing anything about the other
   candidates, and the edge is exactly where the frame needs carrying.

Threshold swept: below 128 the frontier rule barely fires and worst-case sits at 70%; at 250
it is 90–97% across every push-to-talk pattern tested; at 255 (relay always) it is no better,
so the pruning is still doing useful work in the dense case where it matters.

### What still has no mechanism

Simultaneous PTT. Two leaders pressing transmit at the same moment is still unarbitrated —
everything above concerns one voice stream coexisting with control traffic. That is the
remaining half of this question and it is [OQ-0017](#oq-0017)'s territory too.

### What a mechanism has to provide

1. Beacons must reach neighbours reliably while voice is flowing, since the relay chain
   depends on them and re-forms constantly under mobility.
2. It must be deterministic. A statistical scheme that mostly works produces exactly the
   erratic 4–100% behaviour above, which in the field reads as unexplained patchy coverage.
3. It must not break the pipelining rule ([ADR-0002](decisions/0002-tdma-slot-pipelining.md)),
   which requires the very next slot after reception.

The obvious candidate is a set of slots reserved for control and never used by voice, assigned
per node. Sizing falls out of [OQ-0004](#oq-0004): twelve radios beaconing every two seconds
need twelve slots in every 132, which is the 9% airtime already costed. The difficulty is
requirement 3 — a reserved slot in the middle of a relay chain stalls the hop that needed it —
and that interaction is undesigned.

**This is now the largest open item in the project, ahead of the slot budget.** The budget
decides whether a frame can carry voice; this decides whether the mesh can maintain itself
while carrying it.

## OQ-0010
### RX→TX turnaround budget on a half-duplex transceiver

Slot pipelining requires a relay to receive in slot *n*, decide, and transmit in slot *n+1* — the
turnaround happening entirely inside one guard interval. The CC1200's real RX→TX switching time,
plus PLL settling, plus the ramp shaping that EN 300 113 compliance demands, is unmeasured.

If it does not fit, [ADR-0002's reversal trigger](decisions/0002-tdma-slot-pipelining.md#reversal-trigger)
fires. Measure it first in Phase 1, before anything else is built on the bench.

## OQ-0011
### ~~Is full OLSR topology-control dissemination needed at all?~~

**Closed** by [ADR-0007](decisions/0007-packet-switched-frame-architecture.md), 2026-08-21.
**Answer: TC stays.**

The question rested on all traffic being one-to-many broadcast voice, so that MPR-optimised flooding
alone would deliver everything and TC would compute routes nobody used. Addendum 01 removes that
premise: individual addressing, gateways as named destinations, and text, position and configuration
frames all require real routes to specific nodes.

The saving — roughly half of routing overhead on a 19.2 kbps channel — is not available. This makes
[OQ-0004](#oq-0004) more important, not less.

## OQ-0012
### Header field widths and total header size

Addendum 01 fixes which fields exist and explicitly leaves width open: *"be frugal about field
width, not about which fields exist."*
[ADR-0007](decisions/0007-packet-switched-frame-architecture.md#header--straw-man-field-widths)
proposes 34 bits. Every bit of that is a bit of FEC, and per
[OQ-0002](#oq-0002) there are only 14 to spend.

Open per field:

- **Addresses at 8 bits** give 159 handhelds, 32 gateways, 48 groups per network. Ample for a
  12-leader organisation; possibly tight as a product. 6 bits would save 4 bits total and cap the
  network at ~60 nodes. Are addresses per-network or does anything need global uniqueness?
- **Sequence number at 8 bits** gives a 15 s duplicate-suppression window. Is that long enough
  across a partition and rejoin? Too short and duplicates revive; too long and it costs FEC. Phase 0
  can answer this directly from the partition/rejoin scenario.
- **Frame type at 4 bits** reserves 8 values. The addendum asks for "generous" space. Is 8 generous,
  given every extra bit is permanent?
- **TTL at 4 bits** caps at 15 hops, matching the brief's stated maximum exactly — with no
  headroom, and **this is now measured as a hard cap on network diameter, not a safety
  margin**. In a chain at 290 m spacing, delivery tracks the fraction of the group inside 15
  hops almost exactly: 24 radios (23 hops) delivered 66%, 32 radios 50%, 48 radios 31%.
  Rebuilding with a 6-bit TTL — 63 hops, two more header bits — lifted those to 99%, 99% and
  78%. Everything beyond hop 15 is discarded by design and looks, from the field, like the
  far end of the group simply not being covered.

  For twelve leaders this never binds: the worst case is 11 hops. It binds the moment anyone
  asks for a larger group or a longer line, and two bits is a cheap fix in a header that has
  no spare bits ([OQ-0002](#oq-0002)).
- **Sync/preamble at 24 bits** is assumed, not derived. DMR spends 48. This is the largest single
  unverified number in the budget and it is not strictly a header field, but it competes for the
  same bits.
- **Does the header need to be uniform across frame types?** A voice frame in an established stream
  arguably needs less than a beacon. A compressed or implied header for in-stream voice could
  recover a meaningful fraction of the deficit — at the cost of the clean uniform dispatch the
  addendum asks for. Worth evaluating, worth being suspicious of.

### Phase 0 findings — 2026-08-21

**Header width is not an escape route from [OQ-0002](#oq-0002), and this question should stop being
treated as if it were.** Two results from `make budget`:

- With the address map fixed at 8 bits, the only field that can be narrowed without re-deriving the
  map is the sequence number. 8 → 6 bits recovers 2 bits of 264, moving FEC from 14 to 16.
- A header of length *zero* at 4×15 ms / 19.2 kbps / Codec2 3200 still leaves only 48 bits, a 25%
  ratio. The header is 13% of the on-air budget against a deficit of ~88 bits.

So the header can be designed on its own merits — durability, forward compatibility, clean dispatch
— rather than shaved for bits it cannot supply. That is a better position to design from.

What remains genuinely open, now on those merits rather than on budget:

- **Sequence window.** 8 bits gives **15.4 s** of continuous talking before the space wraps —
  measured, not estimated ([OQ-0004](#oq-0004)). Duplicate-suppression entries must therefore
  age out well inside that, or a legitimate new payload is dropped as an echo. Whether it
  survives a partition and rejoin is Phase 0's next question, and this is now the strongest
  constraint on the field.
- **TTL headroom.** 4 bits caps at exactly the brief's 15-hop maximum, with none spare. Deliberate?
- **Frame type space.** 4 bits, 8 reserved. Permanent once radios ship.
- **Address width and map.** `config.h` asserts `MANET_ADDR_BITS == 8` precisely so this cannot be
  changed without re-deriving the range boundaries. Is 159 handhelds / 32 gateways / 48 groups the
  right split, and are addresses per-network or does anything need global uniqueness?
- **Uniform vs compressed header.** An in-stream voice frame arguably needs less than a beacon.
  Now clearly *not* worth the complexity for budget reasons — but possibly still worth it if
  per-frame overhead matters elsewhere. Default to uniform; the burden of proof has shifted.

Settle the rest in Phase 0, against simulated results rather than on paper. The header format is the
one thing here that cannot be changed after radios ship.

## OQ-0013
### Spatial reuse distance and its coupling to slot count

Not in the brief or the addendum. Surfaced while costing the escape routes from
[OQ-0002](#oq-0002), and it decides whether the strongest of them is safe.

Under slot pipelining the originator emits a new codec payload every frame, and so transmits in
slot 0 of every frame. With N slots, by the time it sends payload *k+1*, payload *k* is being
relayed by the node N hops away — in the same slot, simultaneously. **N is therefore the spatial
reuse distance**, and the scheme depends on N-hop separation being enough that the two transmissions
do not collide at any receiver that needs to hear either.

At N=4 that should be comfortable. At N=3 it is marginal, since 2-hop separation is exactly the
hidden-terminal case and 3 hops is only one better. This is the real cost of the 3-slot escape from
OQ-0002 — not hop rate, which the brief incorrectly identifies as the trade.

Phase 0 must characterise: at what reuse distance does a pipelined chain start losing frames, under
realistic (not idealised disc) propagation, and with the irregular hop geometry a dispersed group on
real terrain actually produces? The answer decides between escape routes 3 and 4.

### The scenario that actually matters

Reuse distance is measured in hops, but interference happens in metres, and the two are only loosely
related. The dangerous case is therefore **not** the one intuition suggests.

A group spread across open moorland has long hops — a few kilometres each. Three hops of separation
is then the better part of ten kilometres, and two 5 W handsets that far apart cannot trouble each
other. Reuse is safe and the slot count barely matters.

The risk concentrates in **dense woodland**: terrain blocks the links, so a chain forms — but the
hops are short, perhaps a couple of hundred metres, because that is all that gets through the trees.
Three hops is then well under a kilometre of physical separation, and VHF at 5 W carries far beyond
that even through foliage. The relaying node three hops down the chain may be perfectly audible to
the originator's neighbours while transmitting in the same slot.

So the case to simulate is: a group physically close together, blocked from one another by
vegetation, forming a long chain of short hops. That is a normal Tuesday evening for this operator,
not an edge case. Model it explicitly, with hop length as an independent variable, rather than
trusting a hop count.

### Answered — 2026-08-21. Three slots is not safe.

Measured in the harness, 8-node chain, spacing at 90% of usable range, voice from one end.

**These figures replace an earlier table that could not be reproduced.** The superseded
numbers (99.8% and zero collisions at four slots, in both environments) came from a
2400-slot run under a beacon-scheduling rule that has since been fixed. Under that rule
each radio's beacon landed on its own relay phase and was dropped on priority by
`place()` in `core/src/slot.c`, so only a handful of beacons ever reached the air. **The
zero-collision result was bought by taking the control plane off the air.** Found by
adversarial audit, confirmed by re-running at HEAD. `make reuse` regenerates the table
below, so it cannot drift again.

| | C/I margin | Collisions | End-to-end delivery |
|---|---|---|---|
| **4 slots**, dense woodland | 48.5 dB | 6 | 99.5% |
| **4 slots**, open moorland | 15.3 dB | 81 | **78.5%** |
| **3 slots**, dense woodland | 27.9 dB | 48 | 84.2% |
| **3 slots**, open moorland | **9.6 dB** | 314 | **36.5%** |

Silencing beacons for the voice window separates the two effects completely:

| | Collisions | Delivery |
|---|---|---|
| 4 slots, woodland, beacons silenced | **0** | 99.4% |
| 4 slots, open moorland, beacons silenced | **0** | 99.4% |

**So voice-against-voice spatial reuse at four slots is genuinely safe — zero collisions,
at every chain length tested.** Every collision in the four-slot rows above involves a
beacon. The 78.5% is not a reuse failure; it is [OQ-0009](#oq-0009), the absence of any
channel access mechanism to keep beacons and voice apart, showing up in the static chain
exactly as it does under mobility.

Three slots still fails on reuse alone, and worse: with beacons silenced it remains at
roughly half delivery in open moorland, because the failure there is voice against voice.

The chain collapses from the second hop onward in open terrain at three slots, and the
arithmetic says exactly why. A receiver at hop *k+1* hears its neighbour one spacing
away while the node at hop *k+N* transmits in the same slot, *N−1* spacings away. With
a path-loss exponent of 3.2 that is a ratio of:

- **N=4** → 3 spacings → 10 × 3.2 × log₁₀(3) = **15.3 dB**, comfortably over the 10 dB a
  4FSK demodulator needs to hold the wanted signal.
- **N=3** → 2 spacings → 10 × 3.2 × log₁₀(2) = **9.6 dB**, under it. Both signals are
  lost, every time, everywhere along the chain.

Two corrections to the mechanism, both from the audit and both making the claim harsher.
The margin is **9.41 dB, not 9.63** — `reuse._margin` counts only the nearest interferer,
while a receiver with three radios in the air sums all of them. And "a capture margin that
fails on roughly half of attempts" is wrong: the model has no stochastic component at all.
Capture fails on **100%** of slots where the interferer is up, and the chain settles into a
period-2 half-empty limit cycle. The ~50% figure is right; that explanation of it was not.

**And the environment that fails is the opposite of the one predicted.** Earlier
reasoning in this document held that dense woodland was the danger, because hops are
short there and 5 W VHF carries far beyond a few hundred metres. That was wrong. The
interferer is attenuated by the same woodland, and foliage loss grows *super-linearly*
with distance — so a distant transmitter is punished far harder than a near one and the
margin widens to 48 dB. Open ground follows a plain power law with no such bonus, which
is what leaves the margin thin. **Reuse is safest where propagation is worst.**

One thing this does not settle: the uniform propagation model cannot express a blocked
link, which is [OQ-0019](#oq-0019) and may make the real case worse.

The earlier concern that delivery was only 79–88% even where reuse worked is **resolved** —
it was a harness defect, not a protocol one. See [OQ-0004](#oq-0004).

## OQ-0014
### Authenticating command frames on an infrastructure-free mesh

From [the feature set](feature-set.md). Most of the command-and-control set acts *on* a radio rather
than merely being heard by it: radio check, call alert, remote monitor, and radio disable/enable.

On a repeater system the command arrives through infrastructure the operator controls. On this
network any node can originate any frame, so an unauthenticated disable command is a weapon that
silences the group's radios — including, in the worst case, a radio whose user is in difficulty.

Note this is **authentication, not confidentiality**, and [OQ-0007](#oq-0007) does not cover it.
Encrypting a command does not prove who sent it.

Two things to settle, and the first is not an engineering question:

1. **Should remote disable exist at all for this operator?** For a youth organisation whose radios
   exist for supervision and emergency escalation, a remotely silenceable handset is arguably a
   liability rather than a feature. Radio check and call alert carry no equivalent risk.
   Recommendation in [the feature set](feature-set.md): support it in the architecture, ship it
   switched off, or omit it. Do not default into it.
2. **What authenticates a command?** A shared group key gets most of the way and folds into
   [OQ-0007](#oq-0007). Per-node keys are stronger and collide with hard requirement 8 (shared kit,
   volunteers, minimal training). Reserve the header and payload space now; decide in Phase 3.

## OQ-0015
### Late entry — joining a call already in progress

From [the feature set](feature-set.md). Not in the brief or the addendum.

A radio switched on mid-transmission, or carried into range mid-call, has to work out what is
happening and unmute. DMR solves this by embedding signalling periodically through the transmission,
so a late-joining radio learns the talkgroup and call type without waiting for the next call.

On a mesh this matters *more*, not less: a handset can come into range at any point along a relay
chain, not just within reach of a single repeater. It is also a feature-parity gap rather than a
novel problem — the established standard has it and users expect it.

The difficulty is that the obvious implementation costs bits in a budget that
[OQ-0002](#oq-0002) has already shown to be structurally short. Options: a periodic full-header
frame within a voice stream, a rotating field, or accepting a slower join. Evaluate against the
frame structure chosen for OQ-0002 rather than before it.

## OQ-0016
### Confirmed transactions across a moving multi-hop path

From [the feature set](feature-set.md). A confirmed operation — radio check, call alert, text
delivery receipt, emergency acknowledgement, confirmed private call setup — assumes the path lasts
long enough for a response to return.

Over one repeater hop that is safe. Over five mesh hops at walking pace it is not, and the return
path may differ from the outbound one.

Open: timeout values, retry limits, and whether a retry re-runs route discovery or reuses the stale
path. The failure to avoid is one lost acknowledgement turning into repeated retries across the
whole network on a 4.7 kbps channel — the point at which a convenience feature becomes a denial of
service against voice.

Note the architecture is already right for this: transaction state is endpoint state and lives in
the control payload, not the header, so this costs **zero header bits**. See
[the feature set](feature-set.md#confirmed-operations).

## OQ-0017
### Concurrent call capacity, clustered vs chained

Neither the brief nor the addendum states a concurrent call requirement, and it is not derivable
from anything already written. It needs to be, because the answer differs by an order of magnitude
between two topologies the same group moves through in a single afternoon.

### Clustered — all leaders in direct range

No relaying (every MPR set is empty), so the channel is a single broadcast domain, exactly like a
repeater without the repeater. Slots are independent and each can carry a separate call.

| Structure | Concurrent calls | Spectral efficiency |
|---|---|---|
| DMR Tier 2 repeater | 2 in 12.5 kHz | 1 call per 6.25 kHz |
| 4 × 15 ms in 25 kHz | 4 | 1 call per 6.25 kHz — **equal to DMR** |
| 3 × 20 ms in 25 kHz | 3 | 1 call per 8.33 kHz — **worse than DMR** |

**This is a cost of the 3-slot escape route from [OQ-0002](#oq-0002) that was not previously
stated.** Dropping to three slots costs a concurrent call in the clustered case and takes the
system below DMR's spectral efficiency. It belongs alongside the spatial-reuse cost in
[OQ-0013](#oq-0013) when that decision is made.

### Chained — group dispersed, relaying active

Capacity collapses toward **one call**, and this is a property of multi-hop relaying rather than a
defect in this design.

Under slot pipelining a single call does not occupy one slot — it occupies *every* slot, at
different points along the chain. In a chain with N slots, steady state has nodes 0, N, 2N…
transmitting in slot 0; nodes 1, N+1… in slot 1; and so on. Every slot is busy at every point on
the chain, all of it carrying the same conversation. There is nothing left for a second one.

This is the well-known 1/N throughput scaling of multi-hop networks. Every mesh product has it.

### The comparison that actually matters

The instinct is to compare against a DMR repeater's two slots, but that is not the alternative on
offer where this product is used:

| Situation | DMR today | This system |
|---|---|---|
| At the meeting point (licensed fixed sites) | 2 calls via repeater | 3–4 calls, or bridge to the existing repeater via a gateway |
| Off-site, group clustered | 1 call, simplex | 3–4 calls |
| Off-site, group dispersed | **Nothing works** — this is the problem the product exists to solve | 1 call, reaching the whole group |

Off-site in simplex a DMR handset carries one conversation, to whoever happens to be in earshot. So
the dispersed case is not a capacity regression against current practice; it is a coverage
improvement at equal capacity.

That is an argument for the design, not a reason to leave the question unanswered.

### Can concurrency be bought back in the chained case?

Probably, and the latency budget says there is room. Interleaving two calls through the same slot
structure halves each one's hop rate — a call advances one hop every two slots instead of every
slot. At 3 × 20 ms that is 40 ms per hop, so a 5-hop chain resolves in 200 ms against the 300 ms
criterion. It still passes.

So the answer is likely "two concurrent relayed calls, at a latency cost that the budget can
absorb" rather than a hard one. But **no mechanism for this has been designed**, and it interacts
with [OQ-0009](#oq-0009) (how a node acquires the right to originate at all) and
[OQ-0004](#oq-0004) (where control traffic sits). Do not assume it until it is simulated.

### What Phase 0 must produce

1. Concurrent call capacity as a function of topology, from full cluster to worst-case chain, for
   both 3 and 4 slot structures.
2. Whether interleaved concurrent relayed calls work, and what they cost in latency and
   reconvergence.
3. A stated, defensible capacity figure for each — because "how many people can talk at once" is
   the first question any operator asks, and the honest answer is currently unknown.

## OQ-0018
### ~~The header cannot express the previous hop~~

**Closed** by implementation, 2026-08-21. **The header now carries both.**

Found while writing the harness's forwarding path. MPR flooding relays a frame when the
node it was *heard from* has selected this radio as a relay — RFC 3626 tests the sender
of that particular copy, not the frame's originator. The header carried only `src`, the
origin, which is rewritten nowhere and identifies the frame for duplicate suppression.
With that alone the forwarding rule cannot be evaluated: **the mesh could not relay
correctly at all.**

There is no cheap substitute. A neighbour index instead of an address does not work,
because indices are local to each radio. Relaying "if I am anyone's relay" over-floods a
channel that cannot afford it. Deriving the sender from the payload violates
[ADR-0007](decisions/0007-packet-switched-frame-architecture.md), which forbids the
routing layer from reading payload.

So the header gained a `prev` field of 8 bits and is now 42 bits, 6 bytes. Cost, at
4 × 15 ms: FEC falls from 14 bits to **6**. At 3 × 20 ms: 102 to 94. This makes
[OQ-0002](#oq-0002) worse and it is not optional.

The lesson is [OQ-0012](#oq-0012)'s: the header is the one thing that cannot change once
radios ship, and a field that seemed complete on paper was missing something the
protocol structurally requires. It was caught because the harness drives the real code
rather than a model of it — exactly the property
[ADR-0006](decisions/0006-c-core-python-harness.md) exists to buy.

## OQ-0019
### Uniform-environment propagation cannot express a blocked link

> **Re-graded 2026-08-21 to the most important open item in the project.** Correcting the
> vegetation model ([OQ-0023](#oq-0023)) raised woodland single-hop range from 528 m to
> **4.42 km**. At that range a realistic dispersed group — twelve leaders strung over one to
> three kilometres — is **entirely within direct range of one another**. Measured: 100%
> delivery, and relaying engages only past about five kilometres of spread.
>
> The brief says the opposite, from operational experience: *"Front of group cannot hear
> back of group... A ridge or 200 m of woodland kills a link that would work over open
> ground."*
>
> Both cannot be true, and the brief is the one describing something that actually happened.
> So either handheld range in woodland is far shorter than this model now says, or **the
> mechanism breaking those links is terrain obstruction rather than foliage** — diffraction
> loss over a ridge, which this model contains no representation of whatsoever.
>
> The consequence is uncomfortable and worth stating plainly: **every mesh result in Phase 0
> was produced by a model that manufactured the need for relaying through excessive foliage
> attenuation, not through the mechanism the brief describes.** The protocol conclusions —
> convergence, suppression, election, reuse distance — are about topology and hold whatever
> creates it. The reach and delivery figures are not, and inherit whatever this model gets
> wrong.
>
> **Built 2026-08-21.** `sim/manet/terrain.py` — a height field, and ITU-R P.526 single
> knife-edge diffraction sampled along the ground profile between each pair of radios.
>
> It changes what the simulation is about. A hill 80 m high with groups either side, 2.4 km
> apart: **valley to valley −131 dBm, blocked**; either valley to the hilltop **−106 dBm,
> heard**. Vegetation cannot do that — it saturates at 11 dB. A ridge does it with 23 dB and
> a bit of distance.
>
> And it is the product, stated as geometry: two groups that cannot hear each other, and
> anyone standing between them on high ground who can hear both. A repeater on a hilltop,
> except nobody sited it, nobody licensed it, and it is whoever happens to be up there.
> `sim/scenarios/hill.py` measures it, including the control that matters — **remove the
> hilltop group and the network severs completely**, 4 of 8 reachable and nothing delivered.



Raised by the [OQ-0013](#oq-0013) result, and it bounds how much that result is worth.

The harness applies one propagation environment uniformly to every link, so *blocked*
and *distant* are the same thing. Real terrain does not work that way: two leaders 800 m
apart with a ridge between them cannot hear each other, while a third radio 3 km away
across open ground is perfectly audible to both.

That case is the reason the mesh exists, and the model cannot represent it. Worse, it is
the case where reuse is **most** dangerous — a blocked wanted signal is weak over a short
distance while an unobstructed interferer is strong over a long one, which is exactly
the inversion the capture margin cannot survive.

So the open-moorland chain in OQ-0013 is a *proxy* for terrain blocking, not a
simulation of it. It is a fair proxy — it produces the same thin C/I margin by a
different route — but the real geometry could be worse and cannot currently be tested.

Needs a per-link obstruction model: an attenuation term attached to specific pairs
rather than to the environment as a whole. Until then, treat OQ-0013's answer as a lower
bound on the problem.

## OQ-0020
### How large can the network be?

Raised because the earlier answer in this register was wrong and the correction matters.

Twelve leaders is the example scenario in the brief, **not a system limit**. A mesh whose
capability improves as it grows is the entire premise, so anything that scales with total
node count is a defect rather than a constraint.

Three limits were reported after the first scaling sweep. Two were sizing choices made for
the twelve-person example, and one was a defect in the harness:

- **TTL at 15 hops** — a 4-bit field. Two more bits gives 63. A sizing choice.
- **Neighbour table at 16** — RAM, on a part with 192 KB of it. A sizing choice.
- **Beacon airtime growing linearly with node count** — reported as fundamental. **It is
  not.** The harness gave every radio a globally unique beacon slot, so control overhead grew
  with the size of the whole network. Beacons travel one hop and are never flooded, so their
  slots are spatially reusable exactly as voice slots are. With a bounded pool reused across
  the network, collisions fall to a handful **regardless of size** and overhead depends on
  local density alone.

### What actually bounds it

> ## RETRACTED 2026-08-21 — every figure below is from the 60 ms frame
>
> Measured before `MANET_VOICE_TTL` existed and at a 15 ms slot. The frame is now 160 ms
> with 40 ms slots and voice is TTL-bounded, so **the sixty-hop chain the table describes
> cannot happen**: voice stops at 4 hops.
>
> Specifically retracted:
>
> | Claim below | Status |
> |---|---|
> | "Node count and coverage area are **effectively unbounded**" | **Half false.** Node count, yes — nothing in the protocol scales with N. Coverage is bounded at `MANET_VOICE_TTL` = 4 hops |
> | "Latency 15 ms per hop, linear" | **False twice.** The slot is 40 ms, and the gate chain measures **1.7 slots per hop and rising** (2 hops 1.8, 3 hops 4.2, 4 hops 6.8) because relays wait on the NAMA election |
> | "300 ms criterion reached at about 20 hops" | **False.** The budget is 500 ms ([OQ-0022](#oq-0022)), and at the measured per-hop cost it is reached at **4** |
> | "60 hops still delivered 97.7%" | Unreproducible — TTL stops the frame at 4 |
> | Loss per hop 0.04%, collisions 6 | Unverified against the current build |
>
> The *local density* finding below is unaffected — it concerns beacon slot exhaustion in one
> earshot group, which does not depend on frame duration or TTL.
>
> **Before re-measuring**, `sim/scenarios/scaling.py` needs its chain spacing re-derived for
> the corrected vegetation model ([OQ-0023](#oq-0023)) and the harness must stop reporting hop
> counts that TTL forbids. Until then this question is genuinely open, and the honest current
> answer is: **node count unbounded, reach 4 hops.**

With those corrected, measured across chains from 12 to 200 radios:

| | |
|---|---|
| Loss per hop | **0.04%** — 100% at the talker, 97.7% sixty hops away |
| Collisions | 6, at every size from 12 to 200 |
| Neighbour tables | never saturated in a chain |
| Latency | 15 ms per hop, linear |

**Node count and coverage area are effectively unbounded.** Nothing in the protocol scales
with the size of the network.

**Local density is bounded, and this is real.** Radios within earshot of one another share
one 25 kHz channel and must have distinct beacon slots. Above about 60 in a single earshot
group the pool exhausts and beacons start colliding — 26,152 collisions at 100 co-located
radios. This is not a mesh limitation; sixty radios in one room share one channel under any
technology. It needs distributed slot colouring rather than the naive address-derived
assignment currently in the harness — part of [OQ-0009](#oq-0009).

**Network diameter is bounded by latency, not by the protocol.** At 15 ms per hop the brief's
own 300 ms conversational criterion is reached at about **20 hops**. Beyond that voice stops
being a conversation, regardless of how well the mesh performs — and it performs well: 60
hops still delivered 97.7%, it just took 885 ms.

Twenty hops is a large network. In woodland at 290 m spacing that is ~6 km across; in open
terrain at 5 km spacing it is over 100 km. The number of radios inside that area is limited
only by local density.

### Decisions this forces

1. **TTL should be sized to the latency limit, not below it.** 4 bits caps at 15 hops, under
   the ~20 that latency allows, so it truncates the network before physics does. 6 bits costs
   two header bits and worsens [OQ-0002](#oq-0002) — a genuine trade, not a free fix.
2. **Neighbour table and beacon pool must be sized for expected local density**, not for
   twelve. Both are memory.
3. **Beacon slot assignment must be distributed and local.** The naive scheme works up to the
   pool size and then degrades sharply. [OQ-0009](#oq-0009).

## OQ-0021
### Many-to-many does not work. The system is a one-to-many broadcast tree.

Found by adversarial audit and reproduced directly. **This is the largest gap in the
project and it was invisible because every scenario ever run had exactly one talker.**

### Two talkers destroy each other

Seven-node woodland chain, spacing 0.9× usable range:

> **The single-talker row below is stale — corrected 2026-08-22 (B-08).** It was measured
> before the originator-echo defect was fixed, when a talker relayed its own payload back
> into the network and those spurious copies were buying redundancy that inflated delivery.
> Re-measured on the same chain today:
>
> | | n0 alone |
> |---|---|
> | **Barrage** ([ADR-0011](decisions/0011-barrage-relaying.md)) | **100 / 97 / 96 / 93 / 90 / 88 / 87** — hop 6 at 5.0 slots |
> | Election (what this entry was written against) | 100 / 69 / 51 / 44 / 38 / 37 / 37 — hop 6 at 11.2 slots |
>
> So there is no regression: the barrage figure is far better than anything the election
> ever produced. An adversarial agent reporting `100/61/46/40/36/35/35` as evidence of one
> had measured the *election* baseline, which today reads `100/69/51/44/38/37/37` — the same
> regime within noise, not a decline from 97%.
>
> Note also that spacing between 0.55× and 0.9× of the horizon makes no difference to this
> chain at all: at both, every node reaches its immediate neighbours and nothing further, so
> the topology is identical.

| Talkers | Stream reach |
|---|---|
| n0 alone | 100 / 100 / 99 / 99 / 98 / 97 / 97 *(stale — see above)* |
| n0 **and** n6 | n0 → 100/100/97/**0/0/0/0** · n6 → **0/0/0/0**/97/100/100 |
| n0 **and** n3 | n0 → 100/100/**0/0/0/0/0** · n3 → 0/0/100/100/99/99/99 |

Neither stream crosses the other. Each reaches its own side of the chain and stops dead at
the point where the two meet. The audit reports the same at every chain length from 2 to 9,
**including two radios in direct range of each other with nothing in between** — both
talking, neither heard, zero relays, zero collisions.

**It is not a capacity limit.** The audit hand-placed four talkers on four distinct slot
phases in a twelve-radio cluster and got 77–98% per stream. Four streams fit comfortably in
a four-slot frame. The protocol simply cannot allocate them.

The cause is one line: voice originates only when `slot % slots_per_frame == 0`, with no
per-radio offset, no listen-before-talk, no backoff and no deferral. Beacons have a channel
access rule; **voice has none at all.** This is the other half of [OQ-0009](#oq-0009) — the
half every fix so far has left untouched, because all of them concern one voice stream
coexisting with control traffic.

### Fixed — 2026-08-21. Streams now cross; quality is partial.

Two changes, both decided from local knowledge with nothing negotiated.

**Talkers originate on a phase derived from their own address.** Every talker previously
began on slot 0 of every frame, so two people speaking at once occupied the same slot for
the whole length of their transmissions. A multiplicative hash of the radio's own address
spreads them across the frame — note a plain odd multiplier does not work, since times five
modulo four is just modulo four and every odd address lands on the same two phases.

**A radio carrying two conversations keeps an ear free.** A relay goes out in the slot right
after reception, so a radio relaying one stream transmits in exactly the phase a second
stream arrives on, and is deaf to it. Diagnosed at a mid-chain radio which heard one stream
227 times out of 243 and relayed every one, while hearing the other **27** times.

The discrimination that makes this work without cost: **hearing several neighbours is not
the same as hearing several conversations.** In a chain a radio routinely hears two or three
neighbours relaying the *same* talker, and protecting ears for that blocks the pipelining
rule and costs single-talker delivery about ten points. Keying the rule on the number of
distinct *origins* rather than the number of busy phases gives both.

Seven-node chain, talkers at n0 and n3:

| | Reach |
|---|---|
| n0 → | 100 / 86 / 43 / 42 / 41 / 41 / **41** |
| n3 → | **60** / 61 / 99 / 100 / 99 / 98 / 98 |

Against zero crossing in either direction before. Four simultaneous talkers likewise all
reach across the chain. Single-talker delivery is unchanged at 96.8% worst and the Phase 0
gate still passes 5 of 5.

### The opposite-ends case is a capacity limit, not a defect

Two talkers at the two ends of a chain do not cross at all, and the reason turns out to be
arithmetic rather than a scheduling bug.

**A relay carrying N conversations needs 2N slots per frame** — N to transmit, and N kept
free to listen on, since a half-duplex radio hears nothing while its own PA is keyed.

| Conversations | Slots needed | Of the 4 available |
|---|---|---|
| 1 | 2 | fits, with room for beacons |
| 2 | **4** | exactly full, nothing spare |
| 3 | 6 | does not fit |

Measured directly: every relay in the chain transmits ~238 times in ~240 frames. They are
already at one transmission per frame. The chain divides cleanly, with n0–n2 carrying one
stream and n3–n6 the other, and the boundary radio hearing its own stream 216 times and the
other 20.

Cross-delivery degrades exactly as the talkers separate and share more of the chain:

| Talkers | Cross-delivery |
|---|---|
| 2 hops apart | 49% / 83% |
| 3 hops apart | 42% / 60% |
| Opposite ends of 7 | **0% / 0%** |

**This is [OQ-0017](#oq-0017)'s prediction, confirmed.** That entry reasoned that a chained
topology collapses toward one conversation because a relayed stream occupies every slot
along the chain. It does, and two streams need a frame that does not exist.

Three things could change it, and none is a scheduling fix: more slots (forbidden — four is
forced from both directions by [ADR-0008](decisions/0008-four-slots.md)), a lower vocoder
rate so a conversation needs less than a slot per frame, or accepting that a chain carries
one conversation at a time — which is how push-to-talk works socially anyway, and is worth
weighing before engineering around it.

Attempted and rejected: a periodic listening gap so radios can discover a second stream
they are deaf to. It recovered one direction to 32% and left the other at zero, while
costing single-talker mobility 13–23 points. The asymmetry is the tell — whichever stream
establishes first monopolises the relay, and there is no fairness mechanism.

`multi_talker` in `sim/scenarios/gate.py` covers all of this so it cannot go untested again.

### The destination address is inert

Verified by inspection, and unambiguous. `dst` is written into every header and **never
read**. It appears nowhere in `core/src/*.c` outside `frame.c`; `manet_header_validate`,
the only function that reads it, has no caller outside the test suite;
`manet_addr_is_multicast` has no caller anywhere. The harness never filters on it.

So the addressing scheme of [ADR-0007](decisions/0007-packet-switched-frame-architecture.md)
exists on the wire and controls nothing. Every frame is a broadcast. Private call, group
call and talkgroup selection — baseline expectations in
[the feature set](feature-set.md) — are not implemented in any form, and the header bits
reserved for them are currently paying rent for nothing.

### What this means for the Phase 0 gate

The gate passes 5 of 5, and that result stands as measured — but it measures **one radio
broadcasting to everybody**. It says nothing about two people talking, and nothing about
addressed calls, because no scenario in it exercises either. The criteria came from the
brief and the brief did not ask; that is a gap in the criteria, not a defect in the runs.

### Further audit findings, not yet independently reproduced

Recorded as claims pending verification, not as findings:

- **The beacon slot assignment may depend on a global ordering.** `_beacon_slot_for` uses
  `node.index`, which no real radio has. The audit reports that substituting an
  address-derived slot breaks the clustered case in 4 of 5 seeds, with an 86% chance of two
  of twelve radios colliding. An attempt to reproduce this did not match the gate's own
  baseline and so proves nothing either way. **Needs a faithful test.** If it holds, several
  results rest on a simulator artifact.
- **No duplicate-address detection.** Two radios sharing an address are reported to be
  mutually invisible, silently. Addresses are a global namespace administered outside the
  mesh, which is an infrastructure dependency the brief does not acknowledge.
- **Neighbour table overflow is first-heard-wins with no eviction.** At twenty radios in one
  earshot group the audit reports three of them invisible to every peer.

## OQ-0022
### Which latency budget applies, and therefore how far the network reaches

Raised by [ADR-0009](decisions/0009-frame-structure-with-real-preamble.md). It is a product
decision that has been made implicitly three times in this project, differently each time.

Paying a realistic 8 ms preamble forces longer slots, longer slots cost per-hop latency, and
the usable diameter then depends entirely on which figure the product is held to:

| Budget | Source | Hops at 120 ms / 4 × 30 ms |
|---|---|---|
| 300 ms network only | the brief's own Phase 0 criterion | 5 — passes exactly |
| 300 ms mouth-to-ear | 3GPP TS 22.179 R-6.15.3.2-015 | **2** |
| 500 ms mouth-to-ear | NBWF's alternative requirement | 5 |

These are not variations on a theme. Two hops covers a group that has drifted apart on one
hillside; five covers a dispersed group across a valley. The brief's criterion is the loosest
of the three because it measures propagation only, and it was almost certainly not intended
to exclude packetisation and jitter — but as written, that is what it does.

NBWF hits the same wall from the other side: its own design reaches ~300 ms **before any
relaying at all**, against a 250 ms requirement, and cites 500 ms as the alternative. A
standards body writing for this channel could not meet its own tighter figure.

**Decided 2026-08-21: 500 ms mouth-to-ear.** With the 200 ms frame that gives four usable
hops — 200 ms packetisation, ~60 ms de-jitter and codec, 240 ms of network at 50 ms per hop.
`MANET_VOICE_TTL` is set to 4 to match, and the Phase 0 scenarios are sized to it.

Original discussion: At a 500 ms budget the 90 ms /
4 × 22.5 ms alternate reaches seven hops at 30% FEC; at 300 ms mouth-to-ear nothing reaches
more than three whatever the structure.

## OQ-0023

> ## Closed 2026-08-21 by M-01. The challenge does not survive being run.
>
> FFI discarded the NBWF physical-layer draft's propagation model as giving *"highly
> exaggerated values"* and used Egli with a path-loss exponent of 4. Ours is 3.0 plus an
> ITU-R P.833 vegetation term. That looked like a direct contradiction and it is not.
>
> **Transplanting the exponent alone is wrong, in both directions.** Egli has its own
> intercept and an antenna-height term; a free-space-intercept log-distance model with
> `exponent = 4.0` is not Egli and does not approximate it:
>
> | Model, same 137 dB budget | Single-hop range |
> |---|---|
> | Ours — γ 3.0 + ITU-R P.833 foliage | **4416 m** |
> | Naive transplant — γ 4.0 + foliage | 568 m — *double-counts clutter* |
> | γ 4.0, no foliage | 1044 m |
> | **Egli proper, 1.5 m antennas** | **3967 m** |
>
> **Our model and Egli agree within 11%** when both are driven by the same link budget. The
> alarm came from moving one parameter between two models that do not share the others.
>
> ### The uncertainty is real but it is somewhere else
>
> Reproducing FFI's own published 22 km median from their radio (50 W, 60 MHz, 2.5 m masts)
> gives **43.5 km** under Egli — a factor of two out. Their published figure implies a max
> path loss of 149.6 dB where reconstructing their budget from first principles gives 162 dB,
> so about **12 dB of fade margin they apply and do not itemise**. Applying the same
> correction to our handheld:
>
> | | Our woodland single-hop range |
> |---|---|
> | Our model, and Egli unmargined | **~4.0–4.4 km** |
> | With FFI's implied 12 dB margin | **~1.9 km** |
>
> **So there is a genuine 2× uncertainty, and it lives in the link budget — fade margin —
> not in the propagation exponent.** Only Phase 2 measurement settles it, and this is exactly
> the "median, 10%, 90%" point in [nbwf-lessons.md §4](nbwf-lessons.md): a single number was
> never the right way to quote it (**M-03**).
>
> ### What survives, and what moved
>
> **The protocol conclusions are untouched.** Seven hops at every exponent tried — hop count
> is bounded by latency, not by range, so it does not move. Per-hop delivery is *better* at
> the pessimistic exponent (90.7% vs 84.2%) because hops are shorter and stronger. Spatial
> reuse, convergence, suppression and election are all topology properties and hold whatever
> creates the topology.
>
> **Total reach moves with the range and always did.** 18.2 km at our exponent, 2.3 km at the
> naive pessimistic one. That was always the exposed figure and [OQ-0019](#oq-0019) said so.
>
> **And it caught the same defect class as the gate.** `sim/scenarios/hill.py` had its
> geometry hardcoded in metres — groups 1200 m apart, crest at 1500 m — chosen when the
> horizon was 4416 m. At a shorter horizon the valleys cannot reach the hilltop and the
> scenario silently stops testing relaying. Now derived from measured range, and verified to
> hold at both exponents: 96.7% at γ 3.0, 97.1% at γ 4.0.
>
> `egli_range_m()` is in `sim/manet/radio.py` as a reproducible cross-check rather than a
> one-off calculation.

### ~~The vegetation model was wrong by a factor of eight~~

**Closed by correction, 2026-08-21.** Recorded because every reach figure in this project
rested on it, and because it was caught by someone asking why the number looked low rather
than by any check of ours.

`radio.py` applied `0.2 · f^0.3 · d^0.6` — the early-ITU/Weissberger form — **unbounded**.
That form is specified only to about 400 m. Run to 2 km it charged **87 dB** of foliage
loss.

The correct model is [ITU-R P.833-10](https://www.itu.int/dms_pubrec/itu-r/rec/p/R-REC-P.833-10-202109-I!!PDF-E.pdf)
§2.1 equation (1):

> A_ev = A_m [ 1 − exp(−d γ / A_m) ]

**It saturates**, and the Recommendation states plainly why: *"if the specific attenuation
is sufficiently high, a lower-loss path will exist around the vegetation."* Past some depth
the signal stops going through the trees and goes over and around them. The measured ceiling
at VHF is about 11 dB, not 87.

| Distance | Old, unbounded | ITU P.833 eq (1) |
|---|---|---|
| 500 m | 37.8 dB | 10.3 dB |
| 1 km | 57.3 dB | 11.3 dB |
| 2 km | 86.8 dB | **11.4 dB** |

Parameters are P.833-10 Table 1 and equation (2), fitted to mixed coniferous-deciduous
forest near St Petersburg over **paths from a few hundred metres to 7 km** — a measurement
campaign at the right scale, unlike the 400 m form.

The path-loss exponent for woodland moved 3.0 → 3.5 at the same time. With vegetation now a
separate saturating term, the exponent carries only terrain and diffraction, and has to
carry more of the loss.

### What it changes

| | Before | After |
|---|---|---|
| Woodland range | 528 m | **1334 m** |
| Reach at 4 hops | 1.9 km | **4.8 km** |
| Open moorland | 5933 m (unaffected) | 5933 m |

**So the reach conclusion in [ADR-0009](decisions/0009-frame-structure-with-real-preamble.md)
was too pessimistic by two and a half times.** Four hops covers a group dispersed over nearly
five kilometres of woodland, not two. That is a realistic worst case for the operational
picture in the brief, where a dozen leaders are strung along a path.

It does not change the frame, the FEC shortfall, or the four-hop depth — those come from the
preamble and the latency budget. It changes what four hops is *worth*.

## OQ-0024
### Acquisition preamble — and the LO discipline it depends on

Established in [docs/preamble-budget.md](preamble-budget.md) from the CC1200 datasheet and
user guide: **56 bit-times** — a 4-bit AGC preamble plus a 24-bit sync word, doubled because
the CC1200 sends preamble and sync as 2-GFSK while the payload is 4-GFSK.

Down from 154, which came from NBWF and does not survive reading its source: its own
footnote calls it *"an estimate"*, its itemised fields sum to 5.3 ms not 8, and a third of it
signals which of five PHY modes a burst uses. We have one.

**The condition, and it is a commitment rather than an assumption:** only 8–10 bit-times of
the saving comes from GPS slot timing. The rest comes from a **GPS-disciplined 40 MHz
reference**. With a free-running LO at ±2 ppm the CC1200 needs `TOC_LIMIT ≥ 1` and 2–4 bytes
of preamble, and the figure returns to about 128 — which puts the network back at three hops.
[OQ-0003](#oq-0003) treats GPS as a slot-sync question; it is also a frequency question.

Bench test B in the budget document decides it: PER against injected frequency offset with
`FOC_EN=0`. Flat to ±200 Hz confirms 56; narrower than ±150 Hz means 128.

## OQ-0025
### The PA chain — +16 dBm of modem, 5 W of requirement

The CC1200 is a transceiver, not a transmitter. Its integrated PA reaches about **+16 dBm
(40 mW)**. Every figure in the simulator assumes **+37 dBm (5 W)** — `LinkBudget.tx_dbm`
in [sim/manet/radio.py](../sim/manet/radio.py). That is a 21 dB gap, and the brief has
always carried it: Phase 2 reads *"Custom PCB, PA to 5 W"*.

**It is not a design problem, it is a design job.** The convenient part is that +16 dBm is
close to the drive level VHF RF power modules are specified for — the Mitsubishi RA07M1317M
class (135–175 MHz, 7 W, 12.5 V) wants roughly 50 mW in for full output, so the chain is
plausibly modem → module → harmonic filter → T/R switch, with no discrete driver stage.
Confirm the drive figure and the P1dB back-off against the actual datasheet before
committing; 4FSK is constant-envelope so the module can run near compression, which is what
makes the efficiency numbers in [OQ-0026](#oq-0026) achievable at all.

What must be settled:

- **Drive and linearity.** Constant-envelope permits saturated operation. Verify the module
  reaches 5 W from ≤ 40 mW at 155 MHz and at the top of the temperature range.
- **Supply.** 12.5 V modules mean a 4S pack with a buck, or a 2S pack with a boost. This
  choice sets the enclosure and the weight, for a radio carried all day by a volunteer.
- **Ramp shaping.** The PA must be brought up and down inside the guard. EN 300 113 measures
  adjacent-channel power *during burst transitions*; a hard key splatters. Budget 50–200 µs
  each end against `MANET_GUARD_PERMILLE`'s 3.32 ms — comfortable, but it must be designed,
  and it lengthens the RX→TX turnaround measured in [OQ-0010](#oq-0010).
- **Antenna switching.** Every node relays, so T/R switching happens on almost every slot
  boundary, not on PTT. Switch insertion loss is charged twice per hop and the switch's own
  settling adds to the same turnaround.

**Useful side effect for Phase 1.** The bench EVMs run at 40 mW. At the woodland exponent of
2.97 that is about **1/5 of the product's range**, so a four-hop chain that needs ~5 km at
5 W fits in roughly **1 km** at bench power. Multi-hop behaviour is testable in a single
large field. The protocol does not know the difference.

## OQ-0026
### TX duty cycle — the price of everyone being a relay

> **Corrected 2026-08-21. The alarming figures below are per unit of CHANNEL OCCUPANCY, and
> were read as though they were per unit of wall-clock time.** The simulator's talker never
> releases PTT, so 20.4% worst-node duty means 20.4% *while someone is talking continuously*.
> Scaled to a heavy-but-real 30% occupancy it is 9.0%, and the 10–12 hour requirement is met
> with margin in every credible combination — see [power-budget.md](power-budget.md).
>
> Two further corrections:
>
> - The baseband figure of 1.5 W used below was **invented**, not sourced. It is also now the
>   *dominant* term: at realistic occupancy the always-on receive path costs more than the PA.
>   Measuring it is Phase 1's job and it matters more than PA efficiency.
> - **The mitigation proposed below no longer exists.** Folding a remaining-battery term into
>   `manet_nama_priority()` cannot work: [ADR-0011](decisions/0011-barrage-relaying.md)
>   removed the election from voice relaying entirely, so NAMA governs only beacons (~0.36%
>   duty) and there is no per-frame priority left to steer. There is also nothing to rotate —
>   barrage makes relay load flat rather than concentrated, which fixes the differential-drain
>   problem by levelling it. The only remaining lever on airtime is **fewer relays**, i.e.
>   Controlled Barrage Regions.
>
> What survives unchanged: **thermal**. 5.86 W of PA draw dissipating ~4.4 W inside a plastic
> case is a junction-temperature problem during sustained transmit whether or not the battery
> lasts, and it needs measuring separately from runtime.

**Measured, not assumed.** Running the three-groups-over-a-hill scenario with a single talker
and counting bursts per node from `Simulation.tx_log`:

| | Median node | Worst node |
|---|---|---|
| 3 groups over a hill, 12 nodes | 4.0% | **20.4%** |
| 8 groups strung out, 32 nodes | 2.0% | 17.4% |

A commercial handheld is specified to a **5/5/90** duty cycle — 5% transmit, 5% receive, 90%
standby. The pivotal relay in this network transmits at **four times that**, sustained, and
does so *while nobody is pressing its PTT*. This is the one place where "every handset is
also a relay" costs something a conventional radio never pays.

At 5 W RF and a module efficiency somewhere in 25–45%:

| | η=25% | η=45% |
|---|---|---|
| PA draw, worst node | 4.08 W | 2.27 W |
| Runtime, 4S 3.0 Ah pack (44 Wh), +1.5 W baseband | **7.9 h** | 11.7 h |
| Runtime, 2S 3.5 Ah pack (26 Wh) | **4.7 h** | 6.9 h |

Two consequences, and the second is the interesting one:

1. **Thermal.** Roughly 3–4 W dissipated continuously inside a hand-held plastic enclosure,
   at an ambient that may be a July hillside. Measure junction temperature at 20% duty before
   choosing the module; derating to 3 W RF is preferable to a thermal shutdown mid-incident.

2. **The node everyone depends on is the node whose battery dies first.** Relay load is not
   spread evenly — it is concentrated on whoever is topographically pivotal, which is exactly
   the node whose loss partitions the network. Nothing in the current design notices this.
   The fix is available and cheap: `manet_nama_priority()` already computes a per-slot
   election value, and a remaining-battery term folded into it would rotate relay duty toward
   fuller radios while leaving the election's correctness properties intact. It needs care —
   the priority must stay computable by every neighbour from information they already hold,
   which means battery state has to ride in the beacon. That is a beacon field, a config
   constant, and a term in one expression.

   **Not yet built.** Raised here because it changes what a beacon carries, and beacon layout
   should not be settled twice.

## OQ-0027
### Which VHF band the silicon is characterised for — High Band

The brief lists Mid Band and High Band as interchangeable, decided by whichever 25 kHz simplex
assignment is available. On propagation they are interchangeable. **On silicon they are not.**

[TI's published band list for the CC1200](https://www.ti.com/product/CC1200):

| Band | Status |
|---|---|
| 164–190 MHz, 410–475, 820–950 | Primary, characterised |
| 137–158.3, 205–237.5, 274–316.6 | *"Possible support for additional frequency bands"* — contact TI |

Against the two candidates:

- **VHF High Band, 165.04375–173.09375 MHz** — entirely inside the primary **164–190 MHz** band.
- **VHF Mid Band, 137.9625–165.04375 MHz** — mostly inside the uncharacterised 137–158.3 region,
  and **158.3–165.04 MHz appears in neither list**.

Sensitivity, phase noise, spurious emissions and output power are all specified in the first
case and not in the second — and those are precisely the parameters EN 300 113 tests. Landing
in an uncharacterised band would not stop the radio working; it would mean discovering by
measurement what should have been read off a page, at the point where a conformity failure is
most expensive to fix.

**Engineering position: High Band is the band this hardware is specified in. Mid Band is
workable but costs a characterisation exercise**, because sensitivity, phase noise, spurious
emissions and output power would all have to be measured rather than read off a page — and
those are precisely the parameters EN 300 113 tests.

Recorded as a hardware constraint, not an action. Which band is actually obtained is not an
engineering decision and is out of scope here; this entry exists so that whoever makes it
knows what the part costs in each case.

Two notes attached to the same finding:

- The simulator's `LinkBudget.freq_hz` defaults to **155 MHz**, in the uncharacterised region.
  Protocol results are unaffected — 155 vs 168 MHz is inside the noise of the vegetation model
  — but it should not become the number the RF design is built around.
- **TI's CC1190 range extender does not apply here.** It is advertised on the CC1200 product
  page as the route to +27 dBm, and it is an 850–950 MHz part reaching 500 mW. Neither the band
  nor the power is ours. The 5 W chain is a separately-sourced VHF module, [OQ-0025](#oq-0025).

## OQ-0028
### Do two co-slot relays carrying an identical payload decode?

[ADR-0011](decisions/0011-barrage-relaying.md) removed the per-frame election and took the
per-hop cost from 6.32 slots to 1.00, which took reach from four hops to seven. It rests
entirely on one assumption, now switched on by default and affecting **every delivery figure
in the project**:

> Several relays transmitting the **same payload** in the **same slot** do not jam each other.

This is what a Barrage Relay Network is, and what Glossy measures. It is not verified on our
hardware, and it is the difference between a seven-hop product and a three-hop one.

The model is deliberately conservative — the strongest copy is decoded and the other copies
are merely excluded from the interference sum. No combining gain is claimed. Different
payloads collide exactly as before.

**What has to be true.** Copies must land within a symbol of each other. At 9600 sym/s a
symbol is 104 µs, and a few kilometres of path difference is ~10 µs, so timing is comfortable
with GPS-disciplined slots — the *timing* half is not the risk.

**Carrier frequency offset is the risk.** Two transmitters a few hundred hertz apart produce
a beat, and during destructive periods the composite fades. With 16% FEC there is not much to
ride it out with. This is the same dependency as [OQ-0024](#oq-0024): a GPS-disciplined **LO**,
not merely a disciplined slot clock. Three questions — preamble length, concurrent relaying,
and reach — now hang off the same piece of hardware. [OQ-0003](#oq-0003) should absorb it.

### Bench test

Two CC1120 or CC1200 boards keyed in the same slot with the identical payload, a third
receiving. Sweep, and measure PER at the receiver:

| Sweep | Range | Decides |
|---|---|---|
| Relative power | 0–20 dB | Whether capture alone carries it, or combining is needed |
| **Frequency offset** | 0 to ±1 kHz | **The one that matters.** How much LO discipline is required |
| Timing offset | 0 to ±200 µs | How much of a symbol can be lost before it fails |
| Number of concurrent copies | 2, 3, 4 | Whether it degrades with the crowd — barrage produces 4+ |

Run the frequency sweep first. If PER stays flat to ±200 Hz, ADR-0011 stands and so do seven
hops. If it collapses inside ±50 Hz, the election returns and the product is three hops until
the LO is disciplined.

### How much has to be true — measured, not assumed

The model asserts identical copies *never* jam. Reality will be weaker: capture still needs
some power separation, and the question is how much. Simulated by making identical copies
count as interference whenever they fall within `margin` dB of the wanted copy — `margin = 0`
is ADR-0011's model, `margin = 10` is the pre-ADR-0011 behaviour:

| Separation needed | own group | hilltop | far group | Verdict |
|---|---|---|---|---|
| 0 dB | 98.8% | 96.7% | 96.5% | ADR-0011 as written |
| 1 dB | 98.5% | 98.3% | 98.2% | **holds** |
| **3 dB** | 99.4% | 99.3% | **99.1%** | **holds** |
| **6 dB** | 99.9% | 99.9% | **3.2%** | **collapses** |
| 10 dB | 99.9% | 99.9% | 2.2% | collapses |

**There is a cliff between 3 dB and 6 dB, and nothing gradual about it.** Per-hop cost stays
at 1.00 slots throughout — what fails is not the timing, it is that the frame stops arriving
at all beyond the hilltop.

Two things follow. First, **the decision does not need the strong claim.** If the demodulator
separates identical copies down to 3 dB, seven hops stands; ADR-0011 does not depend on them
never interfering. Second, the [literature review §121](literature-review.md) asked *"what
happens at 0–3 dB?"* — that range is exactly the one that decides it, and the question was
calibrated correctly a long time before this measurement was taken.

Note the 1 dB and 3 dB rows are *better* than 0 dB. Some mutual jamming in the dense hilltop
group suppresses relays that were adding airtime and deafness without adding coverage — which
is Mixer's point in [§111](literature-review.md), that the objective is to **steer** the
number of concurrent transmitters rather than maximise or minimise it. Worth revisiting once
the bench gives a real figure.

**Note this replaces the uncited 10 dB capture assumption** in
[ADR-0008](decisions/0008-four-slots.md) for the identical-payload case only. Different
payloads still use it, and it is still uncited — see the
[literature review](literature-review.md).

## OQ-0029
### FFI looked at our architecture and deferred it

[FFI-rapport 2009/01894](reference-nbwf-ffi-2009-01894.pdf) §4.4, on automatic relaying of
voice without dedicated relay nodes — which is precisely this project's premise, every
handset a relay: they state they are *"not sure if such a protocol is feasible within 25 kHz
bandwidth"*, that extensive simulation is needed to answer it, and that to produce a draft
specification sooner they propose dedicated relays instead. Any node may relay **if configured
to do so**. Configured, not discovered.

**Why this is not fatal.** They carried constraints we do not: 250 ms mouth-to-ear rather than
500, subnets up to 250 nodes, four vocoders without transcoding, simultaneous IP data with
QoS classes, and NATO-wide interoperability. Removing four of five is exactly what turns
infeasible into feasible, and our simulator now shows one slot per hop and 95% delivery over
the hill case.

**Why it stays open anyway.** Our result rests on [OQ-0028](#oq-0028), which is unverified,
and on a propagation model FFI's own numbers say is optimistic ([OQ-0023](#oq-0023)). The one
published team to attempt this concluded it might not fit. That is a fact about the problem,
not about them, and it should be visible next to the delivery figures rather than absent.

**What would close it.** Either the Phase 1 bench confirming OQ-0028 and OQ-0001 — at which
point we have something FFI did not, namely measurements — or finding out what happened to
NBWF after 2011.

> **Amended 2026-08-21: FFI's own fallback is not available to us.** The brief now carries
> **body-worn only** as a hard requirement — every node is carried by a person, and no radio
> is placed, sited or left anywhere, not even a spare handset on high ground. That removes
> dedicated relays as an escape route, which is exactly what FFI retreated to.
>
> It does **not** leave us without a floor. Receiver-decided relaying with the NAMA election
> — the design before [ADR-0011](decisions/0011-barrage-relaying.md) — is still fully
> automatic and fully body-worn. It gives 3–4 hops instead of 7. So the honest worst case is
> **reduced reach, not no product**.
>
> What the requirement does change is the weight on [OQ-0028](#oq-0028). With no dedicated
> relay to fall back on, that single bench measurement is the difference between a seven-hop
> network and a three-hop one, with nothing else to try. It was the most important open
> question already; it is now the only one that can change the product's reach. If automatic relaying was later added to the STANAG, that is the strongest
possible answer. If NBWF was never fielded, that is informative too.

## OQ-0030
### Are we optimising hop count when we should be optimising range per hop?

[FFI §4.1](reference-nbwf-ffi-2009-01894.pdf) reach the opposite conclusion to ours. Their
lowest-rate mode reaches three to four times as far as their highest, and for multicast their
recommendation is to use the **lowest** data rate in order to reach as many nodes as possible
in a single hop. At that rate they need only 1–2 relays to cover more than 50 km.

This project has spent its effort on **hop count** — four hops, then seven, chasing twelve.
[ADR-0011](decisions/0011-barrage-relaying.md) is entirely about making hops cheaper. FFI make
hops unnecessary instead.

The case for their approach is not weak:

- Every hop costs latency, and the latency budget is what caps reach in the first place.
- Every hop costs airtime, which is [OQ-0026](#oq-0026)'s duty cycle problem — already at
  20.4% on the worst node, and made 34% worse by barrage.
- Every hop is another radio that must be in the right place and stay there. A design needing
  seven hops is more fragile than one needing two at equal coverage, and the users here are
  not a fire team holding a formation.
- Link margin is spendable on a lower vocoder rate and more FEC, both of which we want anyway.

The counter is that our radios are 5 W handhelds with helical antennas at head height, not
50 W vehicular sets — our per-hop range in woodland is ~4.4 km against their 22 km, so the
same coverage genuinely needs more hops. And the terrain case in
[ADR-0010](decisions/0010-terrain-diffraction.md) is a blocking problem, not a range problem:
no amount of link margin gets through a ridge, only a radio standing on it.

**So the honest position is that both are true and we have never traded them off explicitly.**
The bench measurement that decides it is [OQ-0001](#oq-0001) — if a lower-rate mode buys
substantially more range in the same channel, the right answer may be fewer, longer hops with
a lower vocoder, not more, cheaper hops. That is a different product.

## OQ-0031
### What happens when GPS goes away

Nearly every advantage this design has over [NBWF](nbwf-lessons.md) traces to one thing: FFI
were required to work without GNSS, and we are not. That makes GPS dependence a fair question
and it has never been answered.

**What actually breaks, in order of how quickly.**

*Slot timing* degrades gradually. The whole `MANET_GUARD` of 3320 µs is the error budget, and
it is consumed by the **relative** drift between two nodes — so twice the per-node figure:

| Reference | ppm | Relative drift | Holdover before slots overlap | Cost |
|---|---|---|---|---|
| Cheap XO | 20 | 40 µs/s | **83 s** | ~£0.20 |
| Standard TCXO | 2 | 4 µs/s | **14 min** | ~£1 |
| Good TCXO | 0.5 | 1 µs/s | **55 min** | ~£3 |
| MEMS OCXO | 0.05 | 0.1 µs/s | **9.2 h** | ~£15 |
| OCXO | 0.01 | 0.02 µs/s | 46 h | ~£40 |

*Acquisition* degrades next. [OQ-0024](#oq-0024): a free-running LO at ±2 ppm forces
`TOC_LIMIT ≥ 1` and the preamble goes from 56 bit-times to ~128, which costs hops.

*Concurrent relaying fails hardest.* [OQ-0028](#oq-0028) rests on identical co-slot copies
combining, and carrier frequency offset between transmitters is precisely what breaks that.
Worse, the mobility stress test measured a **mean of 3.7–9.4 and a maximum of 11 identical
copies in one slot** — CFO risk scales with the number of co-transmitters, not with two. So
GPS loss does not merely degrade barrage, it is the mechanism most likely to collapse it.

**The requirement we actually have is much weaker than FFI's, and that is the answer.**

They needed the waveform to work with **zero** GNSS anywhere, because their threat model is
jamming. Ours is not. The realistic UK loss cases for a youth-organisation leader — dense
canopy, a deep valley, inside a building — are **local and partial**. In a mesh, that is the
easy case: **one node with a fix is enough**, and time propagates from it through the same
beacons that already carry neighbour state.

> **Answered in design 2026-08-21 by [ADR-0012](decisions/0012-network-time-authoritative.md).**
> The concern is not the outage, it is that a safety system cannot have a single point of
> failure in a service nobody involved operates — and spoofing is worse than jamming, because
> a spoofed radio transmits confidently in the wrong slot. The answer was already in the
> architecture: Glossy is titled *"Efficient Network Flooding **and Time Synchronization**"*
> and gets sub-microsecond sync **implicitly, from the flood itself**. ADR-0011 bought that
> and paid only for the flooding. Network time becomes authoritative, GPS advisory and
> sanity-checked. The table below still governs the case where a radio hears nobody at all.

So the design is three layers, and only the first exists today:

1. **GPS-disciplined** where a fix is available. Built.
2. **Network time transfer** — a node without a fix slaves to a neighbour that has one,
   with a hop count so error accumulates predictably. Not built. This is the layer that turns
   a hard dependency into a soft one, and it is cheap because the beacon already exists.
3. **Holdover** when nobody has a fix. Bounded by the oscillator, per the table above.

**The decision this forces is a component choice, and it is worth making early.** A £3
good TCXO gives 55 minutes of total-blackout holdover; £15 of MEMS OCXO gives 9.2 hours,
which covers a whole event. Against a radio that already needs a PA, a GPS receiver and an
STM32, £15 is not the expensive part — and it also directly serves
[OQ-0024](#oq-0024) and [OQ-0028](#oq-0028), both of which want a disciplined reference for
reasons that have nothing to do with GPS outages.

**Note this is a different question from disciplining the LO.** Holdover is about how long
the reference stays good with no correction; [OQ-0003](#oq-0003) and OQ-0024 are about
whether the 40 MHz reference is steered by GPS at all. The same part answers both, which is
why they should be decided together.

## OQ-0032
### Dense cover spends the hop budget before the group ends

Every scenario in this project is a variation on the repeater triangle: groups spread over
kilometres, terrain doing the blocking, hops long and few. That is the case the product was
conceived for and it works. **It is not the only case, and the gate is blind to the other
one.**

In thick woodland the links shorten, so the same group needs *more* hops to span the *same*
ground, and `MANET_VOICE_TTL` runs out before the group does.

Twelve leaders in a line, worst-case delivery, TTL 7:

| Cover | 1 hop | 0.5 km | 1 km | 2 km | 3 km | 5 km |
|---|---|---|---|---|---|---|
| light — our default | 4416 m | 98% | 99% | 99% | 99% | 99% |
| moderate | 1500 m | 99% | 99% | 99% | 98% | 95% |
| dense | 700 m | 99% | 99% | 96% | 92% | **0%** |
| very dense | 350 m | 99% | 96% | **0%** | **0%** | **0%** |

**The corner that matters is twelve leaders over 2 km of very dense cover** — an entirely
ordinary way for a group to walk a forest trail:

```
radios reachable through the mesh : 12/12
radios actually receiving voice   :  8/12
```

**The radio path is intact end to end.** Every leader is connected to the next. Voice stops
anyway, at radio 7, because the hop budget is exhausted before the group is. The last four
people are unreachable and nothing in the network is broken.

### Why it cannot be fixed by raising the TTL

Seven hops already costs 460 ms of the 500 ms mouth-to-ear allowance
([OQ-0022](#oq-0022)). There is no slack. **Dense cover converts a latency limit into a
coverage limit**, and the conversion rate is unfavourable: at a 350 m horizon the seven hops
buy 2.4 km of ground, where at our default they buy 30 km.

### What would actually help

- **A shorter frame.** At 110 ms — which 22.4 kbps with Codec2 2400 buys, both bench
  questions ([OQ-0001](#oq-0001)) — the same latency allowance holds **twelve** hops rather
  than seven. In very dense cover that is 4.2 km instead of 2.4 km. This is the strongest
  argument yet for the bit-rate work, and it is not the argument that was being made for it.
- **Knowing the number.** If the answer is "in thick woodland keep the group inside 2 km",
  that is an operational instruction somebody can actually follow — but only if it is
  measured rather than discovered.
- **Not [OQ-0030](#oq-0030)'s fewer-longer-hops strategy**, which helps in the open and does
  nothing here: in dense cover there are no long hops available to take.

`sim/scenarios/dense_cover.py`. Raised by the user, from the observation that the triangle
is not the only geometry a group can be in.

## OQ-0033

> ## Closed 2026-08-22. It was a simulator defect, not a protocol one.
>
> **`Channel.decode` summed interference over every other transmission in the network,
> including ones far below the demodulator's floor.** In a twelve-radio chain there are one
> or two such transmissions and it changes nothing. In a hundred-radio mesh there is a
> **median of eighteen per slot**, and they contributed a **median of 100% of the
> interference power** — so a receiver was being jammed entirely by signals it could not
> hear.
>
> Sub-floor signals are not ignored on principle. They are already inside the sensitivity
> figure, which is defined against the receiver's own noise; adding them again counts the
> same noise twice.
>
> | Same 38 km of ground | before | after |
> |---|---|---|
> | 100 radios | 46.7% | **92.9%** |
> | 200 radios | 50.7%, 1/200 usable | **99.7%, 200/200 usable** |
>
> **Adding radios now improves the network monotonically**, which is what a mesh is for and
> what the plateau at 50.7% was contradicting.
>
> **It also moves the headline hop figure, twice corrected now.** At hop 7 the chain
> delivers 90.5% where it delivered 84.0%, so the usable depth goes from 4 hops back to
> **the full 7** — the earlier correction was itself an artefact of this bug. Per-hop:
> 98.1 / 96.7 / 95.3 / 94.0 / 92.1 / 91.5 / 90.5.
>
> Spatial reuse also improved: collisions 39 → 8, end-to-end 92% → 94%. The hill and the
> gate are unchanged, having too few radios for the bug to bite.
>
> **Found because the user pushed back on a mesh that got worse as radios were added.** Three
> hypotheses were proposed and disproved first — density, spatial reuse at the four-slot
> distance, and hop-4 relays reaching the talker's neighbours.

### Half of all payloads die at the talker's first hop, and adding radios does not fix it

Raised by the user, from the observation that a mesh which gets *worse* as radios are added
is not behaving like a mesh. It does not, and the cause is not what it looks like.

**Adding radios to the same ground barely helps.** 100 radios over 38 km:

| radios | neighbours each | mean delivery | usable (≥90%) |
|---|---|---|---|
| 40 | 1.9 | 12.0% | 4/40 |
| 100 | 5.5 | 46.7% | 1/100 |
| 200 | 11.5 | **50.7%** | **1/200** |

Five times the density, and it plateaus near 50%. That is not a connectivity problem.

**The loss is entirely at the first hop.** Delivery against hop depth, 100 radios:

| hop | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| delivered | **49.0%** | 48.8% | 48.5% | 47.0% | 45.4% | 43.7% | 40.0% |
| implied per hop | **49.0%** | 99.4% | 99.5% | 96.9% | 96.6% | 96.1% | 91.7% |

**Hops two through seven are near perfect.** The mesh is working. Half of everything the
talker says never reaches its own neighbours.

**What happens at that first hop**, measured at the talker's five direct neighbours:

| | |
|---|---|
| collided | **52.4%** |
| heard the talker | 46.5% |
| deaf, own PA keyed | 0.7% |

So it is collision, not deafness and not range. The interferers are radios 3.9–7.0 km from
the talker — **some of them the talker's own neighbours** — and the wanted signal arrives
with a **median margin of −0.3 dB** against them. Capture needs 10 dB. Nothing survives.

### What has been ruled out

- **Density.** More radios makes it slightly better, then plateaus. Not the cause.
- **Neighbour-table overflow.** Fixed under [B-05](backlog.md); the plateau is unchanged.
- **Different payloads colliding** — pipeline self-interference at the 4-slot spatial reuse
  distance ([OQ-0013](#oq-0013)). Measured: a radio hears two or more *different* payloads in
  the same slot in **0.1%** of slots. Too rare to explain a 52% loss.
- **Hop-4 relays reaching the talker's neighbours.** Zero audible pairs in either a chain or
  a scatter.

### What has not been established

**Which transmission is actually winning the receiver.** `Channel.decode` locks to the
strongest signal and treats everything else as interference *unless it carries the identical
payload* ([ADR-0011](decisions/0011-barrage-relaying.md)). If a neighbour is closer to
another relay than to the talker, it may be locking onto the louder older copy and losing
both. That is a hypothesis and **it is not yet measured** — three earlier hypotheses about
this failure were each disproved by the next measurement, so this one is recorded as
untested rather than presented as the answer.

**Blocking, because every scattered-mesh figure in [the atlas](../sim/scenarios/atlas.py)
inherits it**, and because a 50% ceiling that density cannot lift is either a real protocol
defect or a simulator defect. Both matter and neither is acceptable to leave unexplained.

## OQ-0034
### The relay rule refused a sender it had merely forgotten

Closed on the day it was raised; recorded because the shape of the mistake is worth keeping.

`manet_mpr_should_relay` opened with *"do I have an entry for this sender?"* and returned
**false** on a miss — stopping the frame. Three lines below, the same function returns
**true** when the sender's entry exists but carries no advertised coverage, on the explicit
grounds that *"if we have never heard what the sender can reach, we cannot prune, so we
relay"*.

Both are the same situation — we do not know what the sender covers — and they were answered
opposite ways. The comment justifying one was sitting directly beneath the code doing the
other.

**Why it bit.** `MANET_MAX_NEIGHBOURS` is 16 and a radio in a moderately dense group hears
more than that, so eviction drops a sender on a perfectly good link and every frame that
sender relays dies at whoever forgot them. Measured on a **−108 dBm** link, 8 dB clear of
sensitivity.

**The general lesson, and it is the third time today:** a table that silently forgets things
produces failures that look like protocol defects. [B-05](../docs/backlog.md) was the first
symptom, [B-13](../docs/backlog.md) the second, and the 40-radio regression blamed on
[OQ-0033](#oq-0033) the third. `MANET_MAX_NEIGHBOURS` deserves re-deriving from the density
the product actually has to survive rather than from the twelve-leader case it was sized for.

## OQ-0035

**NAMA is the only thing that needs the absolute slot number. Could it need less?**

**Status:** open · **Phase:** 1 (wire format) · **Blocks:** W-02 cold start

A radio joining a network with no GPS can hear burst edges, so it can align to slot
*boundaries*. Nothing on the wire tells it the slot *number* — `manet_header_t` carries src,
prev, dst, type, seq, ttl and prio, and no timestamp, epoch or slot field. Today the number is
derived, not transmitted: `manet_slot_at()` is absolute time divided by the slot length, and
every radio with a GPS fix computes the same value independently. That is ADR-0012 layer 4,
the only layer built.

What makes this worth an entry is that **most of the protocol does not need the absolute
number at all**:

| consumer | needs | derivable by listening? |
|---|---|---|
| `manet_voice_phase()` | the radio's own address; no time | yes — needs no clock |
| `manet_slot_is_control()` | slot number **mod 8** (`MANET_SIGNAL_SLOT_PERIOD`) | yes — beacons land 1-in-8 |
| `manet_nama_wins()` | the **absolute** number, hashed with the address | **no** |

Voice phase is a pure function of address. The B-15 signalling reservation needs only the
residue, which a newcomer could infer by observing where beacons fall. **NAMA is the single
consumer forcing absolute agreement**, because `manet_nama_priority(node, context)` hashes the
slot number itself: two radios disagreeing about the number compute different priorities, and
the collision-free guarantee is void with no error indication — the same silent-failure shape
as B-05, B-13 and B-15.

**The question.** Could NAMA's context be modular — the slot number mod some N — reducing the
cold-start requirement from "agree on an absolute count" to "agree on a phase", which is
observable from the air?

**Why it is not obviously yes.** `nama.h` argues the opposite direction: context diversity is
what stops the same radio winning repeatedly, which is why slot ownership was moved up to the
superframe rather than the four-slot voice frame. A modular context has a fixed number of
distinct values and would repeat. The trade is unmeasured.

**What would settle it.** Sweep NAMA with context taken mod N for N across a range, measuring
election fairness, beacon collision rate and neighbour-table convergence against the current
absolute-context behaviour. If a modular context holds up, layer 1 of ADR-0012 gets materially
easier — a newcomer could reach full participation by listening rather than by being told the
time. If it does not, then W-02 must carry the slot number explicitly, and that is a wire
format change to price in [ADR-0012](decisions/0012-network-time-authoritative.md).

Raised 2026-08-22 while tracing what the B-15 reservation depends on.

## OQ-0036

**How much of shadowing is shared between nearby radios, and how much is each radio's own?**

**Status:** open · **Phase:** 1 (bench) · **Relates to:** M-06,
[ADR-0010](decisions/0010-terrain-diffraction.md)

Shadowing is applied per link as two terms whose variances sum to σ = 7.0 dB:

- **shared**, quantised to `SHADOW_GRID_M` = 100 m — two radios standing near each other look
  through the same stand of trees at the same hillside, so they meet largely the same obstruction;
- **local**, keyed on exact positions — the tree one person is beside, the dip they stand in,
  which way their body is turned. Theirs alone, different for every link.

`SHADOW_SHARED_FRACTION = 0.5` splits them evenly. **That 0.5 is a modelling choice, not a
measurement.** Published correlation coefficients for land-mobile VHF run roughly 0.3 to 0.8
depending on environment and antenna height, and 0.5 sits in the middle without being derived
from anything.

**Why it matters, measured.** The split barely moves how likely any *single* link is to work —
mean links up between two groups of four at 0.9× horizon is 9.4 of 16 whichever value is used.
What it decides is whether links fail *together*. With the shared term carrying everything
(fraction 1.0, which is what the model did before this entry), a whole group boundary goes dead
8.2% of the time, because four radios in one 100 m grid cell take a single roll of the dice. At
0.5 that falls to **0.3%**, a factor of 27, and at 0.0 it is nil.

So the fraction is a direct control on how brittle clustered groups look — and clustered groups
are the topology the product is for. It should be measured rather than assumed.

**What would settle it.** Two handsets a fixed distance from a third, walking a route together
at varying separations, logging RSSI on both. The correlation between the two received levels as
a function of their separation gives both the coefficient and the decorrelation distance
directly. It is the same field exercise that would validate ITU-R P.833 against our own woodland,
so it costs one outing, not two.

**Until then**, quote clustered-group figures as resting on an assumed correlation, and note that
the sensitivity is one-sided: a higher shared fraction makes groups look more fragile, never less.

Raised 2026-08-22 after a scale scenario collapsed from 32/32 radios to 4/32 under the
all-shared model, and the cause turned out to be the grid being coarser than a group.

## OQ-0037

**A radio above the treeline still pays full vegetation loss.**

**Status:** open · **Phase:** 1 (bench) · **Relates to:** OQ-0023, ADR-0010

Antenna height now enters the path loss (Egli's `-20 log10(h_tx · h_rx)`, applied as a delta
from the 1.5 m both-on-foot case the distance model is calibrated for), bounded by the 4/3-earth
line-of-sight horizon. A radio on an 80 m ridge gains about 35 dB, roughly seven times the range,
and in the atlas's eight-groups-over-a-ridge scenario the one radio standing on the crest goes
from hearing 11 of 31 others to hearing **all 31**.

**What is still wrong.** `Environment.path_loss_db` charges the ITU-R P.833 vegetation term over
the whole path regardless of height. The fit assumes trees of mean height 16 m
(sim/manet/radio.py:90). A radio 80 m up is well clear of the canopy and its path to a distant
valley spends most of its length above the trees, so charging it the full ~11 dB is wrong.

**Which way the error runs.** Conservative. Elevation is worth *more* than the model now credits,
not less, so nothing quoted is over-optimistic because of this. That is why it is recorded rather
than fixed in the same change — the safe direction, and it needs a real treatment rather than a
guess.

**What a real treatment looks like.** Vegetation loss in proportion to the fraction of the path
that actually passes through canopy, which needs the ground profile (already sampled for
diffraction in `terrain.profile`) and a canopy height per environment. Not difficult; it just
should not be invented alongside the height term without something to check it against.

**Also unmodelled, and NOT conservative:** paths beyond the line-of-sight horizon are charged as
a hard failure rather than as over-the-horizon diffraction, which is a real mechanism that
carries real signal. On foot the horizon is 10.1 km against a 4.4 km woodland range, so it never
binds; it only matters once a radio is high enough for the gain to reach that far. Two radios on
an 80 m ridge would be cut off at 42 km where something would in fact get through.

Raised 2026-08-22, while adding the height term — noticed because the model had a hill it could
only ever treat as an obstruction.
