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
| [OQ-0001](#oq-0001) | Achievable gross bit rate at 25 kHz on CC1200 | Phase 1 | Slot count, vocoder rate, FEC strength | Open |
| [OQ-0002](#oq-0002) | The slot budget does not close — structurally, once itemised | **Phase 0** | Everything downstream of the MAC | Open |
| [OQ-0003](#oq-0003) | Synchronisation: GPS-disciplined or network-derived | Phase 0 (design), Phase 2 (proof) | Guard interval size, canopy/indoor operation | Open |
| [OQ-0004](#oq-0004) | Beacon interval, and where control traffic lives in the slot structure | **Phase 0** | Channel overhead, reconvergence time | Open |
| [OQ-0005](#oq-0005) | Slot count — is 4 right? | Phase 0, revisit Phase 1 | Per-slot payload, spatial reuse margin | Open |
| [OQ-0006](#oq-0006) | VHF Mid Band or High Band | Ofcom enquiry | Nothing technical | Open |
| [OQ-0007](#oq-0007) | Encryption | Phase 3 | BOM, key management UX | Open |
| [OQ-0008](#oq-0008) | In-house or contracted development | Commercial | Schedule and cost | Open |
| [OQ-0009](#oq-0009) | Channel access: who owns a slot, across four priority classes | **Phase 0** | The MAC is not fully specified without this | Open |
| [OQ-0010](#oq-0010) | RX→TX turnaround budget on a half-duplex transceiver | Phase 1 | Guard interval, therefore payload, therefore OQ-0002 | Open |
| [OQ-0011](#oq-0011) | ~~Is full OLSR topology-control dissemination needed at all?~~ | — | — | **Closed** — TC stays |
| [OQ-0012](#oq-0012) | Header field widths and total header size | **Phase 0** | ~~OQ-0002~~; the header format is forever | Open — no longer blocks OQ-0002 |
| [OQ-0013](#oq-0013) | Spatial reuse distance and its coupling to slot count | **Phase 0** | Whether the 3-slot escape from OQ-0002 is safe | Open |
| [OQ-0014](#oq-0014) | Authenticating command frames on an infrastructure-free mesh | Phase 3 (design now) | Whether radio disable can safely exist at all | Open |
| [OQ-0015](#oq-0015) | Late entry — joining a call already in progress | **Phase 0** | Header/framing; costs bits that do not exist | Open |
| [OQ-0016](#oq-0016) | Confirmed transactions across a moving multi-hop path | Phase 0 | Every confirmed feature in the set | Open |
| [OQ-0017](#oq-0017) | Concurrent call capacity, clustered vs chained | **Phase 0** | Feature parity with DMR; the 3-vs-4 slot decision | Open |

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
| 3 | **3 slots × 20 ms** | **Currently the strongest.** Leaves 102 bits for FEC — a 45% ratio, landing on the brief's own 47% target. Costs spatial reuse margin, not latency ([OQ-0013](#oq-0013)) |
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
without touching the vocoder. It remains contingent on [OQ-0013](#oq-0013).

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

## OQ-0005
### Slot count

Four is derived from Codec2 3200 plus 47% FEC at 19.2 kbps — and that derivation is exactly the one
that does not close ([OQ-0002](#oq-0002)). Revisit once the gross rate is measured.

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
- **TTL at 4 bits** caps at 15 hops, matching the brief's stated maximum exactly — with no headroom.
  Deliberate or accidental?
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

- **Sequence window.** 8 bits gives ~15 s. Long enough across a partition and rejoin? Phase 0's
  partition scenario answers this directly, and it is now the strongest constraint on the field.
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

Note this is analysis, not a measured result. It should be confirmed or demolished in simulation
before anything is built on it.

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
