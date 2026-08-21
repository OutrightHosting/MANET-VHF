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
| [OQ-0001](#oq-0001) | Achievable gross bit rate at 25 kHz on CC1200 | Phase 1 | **THE critical path** — the budget cannot close without it | **Open, blocking** |
| [OQ-0002](#oq-0002) | The slot budget does not close — structurally, once itemised | **Phase 0** | Everything downstream of the MAC | Open |
| [OQ-0003](#oq-0003) | Synchronisation: GPS-disciplined or network-derived | Phase 0 (design), Phase 2 (proof) | Guard interval size, canopy/indoor operation | Open |
| [OQ-0004](#oq-0004) | Beacon interval, and where control traffic lives in the slot structure | **Phase 0** | Channel overhead, reconvergence time | Open |
| [OQ-0005](#oq-0005) | ~~Slot count — is 4 right?~~ | — | — | **Closed** — [ADR-0008](decisions/0008-four-slots.md), 4 is forced |
| [OQ-0006](#oq-0006) | VHF Mid Band or High Band | Ofcom enquiry | Nothing technical | Open |
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
| [OQ-0019](#oq-0019) | Uniform propagation cannot express a blocked link | **Phase 0** | **BLOCKING.** Whether any reach figure means anything | **Open, blocking** |
| [OQ-0020](#oq-0020) | How large can the network be? | **Phase 0** | Sizing of TTL, tables and beacon pool | Open — earlier answer was wrong |
| [OQ-0021](#oq-0021) | Many-to-many — streams now cross; `dst` still inert | **Phase 0** | Addressed calls; concurrent-stream quality | Open — no longer total failure |
| [OQ-0022](#oq-0022) | ~~Which latency budget applies~~ | — | — | **Closed** — 500 ms mouth-to-ear |
| [OQ-0023](#oq-0023) | ~~Vegetation model wrong by 8×~~ | — | — | **Closed** — ITU-R P.833-10 eq (1) |

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
### VHF Mid Band or High Band

Propagation is near-identical. Availability of a 25 kHz simplex assignment decides it. This is an
Ofcom enquiry, not an engineering question, and nothing downstream waits on it — but it should be
raised early, because it is also where the answer to "is a 25 kHz simplex assignment obtainable at
all" comes from, and that one *is* load-bearing
([ADR-0001 reversal trigger](decisions/0001-narrowband-vhf-licensed-spectrum.md#reversal-trigger)).

Now also carries a second question: the Technically Assigned licence for gateway sites
([Addendum 01 §4](addendum-01-packet-architecture.md#4-node-types)). Conventional and well-trodden,
but worth raising in the same enquiry rather than a separate one.

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

| Talkers | Stream reach |
|---|---|
| n0 alone | 100 / 100 / 99 / 99 / 98 / 97 / 97 |
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
