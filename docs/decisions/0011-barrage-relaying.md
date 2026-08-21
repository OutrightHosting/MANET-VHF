# ADR-0011: Barrage relaying — identical concurrent copies combine, so the election goes

**Status:** Accepted
**Date:** 2026-08-21
**Phase:** Phase 0
**Amends:** [ADR-0002](0002-tdma-slot-pipelining.md), which promised one hop per slot and was
not getting it. Supersedes the NAMA election *for voice relays only*; NAMA still owns beacons.

## Context

[ADR-0002](0002-tdma-slot-pipelining.md) states the design is a Barrage Relay Network,
citing TrellisWare TSM and Glossy, and promises **one hop per slot**. The implementation was
not delivering it. Instrumented over the hill scenario:

```
why each relay attempt was rejected
  lost election       21316   81.8%
  relayed              3496   13.4%
  scheduler refused    1010    3.9%
  gave up               234    0.9%

slots of delay before a relay went out: mean +5.32  ->  6.32 slots per hop
```

Only 11.3% of relays achieved perfect pipelining. Contender sets were 5–9 nodes, so a radio
waited roughly six slots for its turn in an election it had to win before it could forward.

**The election was answering the wrong question.** It asks "may *I* transmit in this slot?"
when the question is "is *anyone* holding this frame going to move it forward now?"

And it existed for one reason, recorded in the code: four radios on a hilltop all relaying in
the same slot measured 1.5% delivery. But that measurement came from a channel model in which
`decode()` counted **every** other transmission as interference — including transmissions
carrying the **identical payload**. In a barrage relay network that is not a collision; it is
the mechanism. Several relays emitting the same waveform in the same slot present a receiver
with something that looks like multipath.

So a modelling error had produced a real architectural retreat, and the retreat cost
6.32 slots per hop against a promised 1.

### This ADR is late. The literature review said all of it.

[docs/literature-review.md](../literature-review.md) called this precisely and was not acted
on. Recorded here because the failure was one of process, not of information:

- **§111** — *"In a pipelined flood, pruned and unpruned relays alike transmit in slot n+1 —
  **pruning frees no slots**. What it buys is energy and a better chance the capture power
  condition holds… extra concurrent relays are* sender diversity*, a reliability asset.
  Mixer's objective is the right one: **steer the number of concurrent transmitters**…
  rather than minimising them."*
- **§119** — *"we chose 4FSK… and then built receiver-decided pruning, a frontier rule and a
  notional stagger **to prevent the very thing that makes the architecture work**."*
- **§31** — the timing margin, already derived: *"Our 4FSK symbol at 9600 baud is 104 µs —
  roughly 200× the slack… every concurrent relay lands inside the capture window by two
  orders of magnitude. Capture in our system is decided by power ratio alone… Record that as
  designed margin."*
- **§121** — the fork, stated explicitly, with the measurement that resolves it: prune to one
  relay, *or* "keep several and steer the power spread", decided by what a demodulator does
  with two identical co-slot copies at 0–3 dB. That measurement is now
  [OQ-0028](../open-questions.md#oq-0028) and it should have been taken first.

NAMA was recommended by the same document — at **§53, for channel access**. It was applied to
voice relaying instead, which §111 had already ruled out. A correct recommendation used on the
one thing that document said it would not help.

### One caveat from §119 that this ADR must respect

*"BRN works because concurrent identical relays are made to **combine** at the receiver — CPM
plus per-node random phase dithering plus a long-blocklength code and MLSE. We chose 4FSK,
which has no such mechanism."*

True, and it is why the channel change below claims **no combining gain**. What is available
to 4FSK is capture, and §31 establishes we sit two orders of magnitude inside the capture
*window*, leaving only the power *ratio* in question. The model asserts the weaker thing —
identical copies do not jam each other — not the stronger thing that BRN's PHY buys.

## Decision

Two changes, which only work together:

1. **`Channel.CONCURRENT_IDENTICAL`** — copies of the same payload (same origin, same
   sequence) are excluded from the interference sum. Deliberately conservative: the strongest
   copy is decoded and the others merely stop jamming it. **No combining gain is claimed**,
   though Glossy measures one. Different payloads collide exactly as before, so
   [OQ-0013](../open-questions.md#oq-0013)'s spatial-reuse result is untouched.

2. **`Simulation.BARRAGE_RELAY`** — a radio that should relay does so in the very next slot,
   unconditionally. No election. This is ADR-0002's pipelining rule as originally written.

Multipoint-relay pruning is unchanged and still decides *whether* a radio relays. Barrage
decides *when*, and the answer is always "next slot".

## Consequences

Measured, hill scenario, three groups over a ridge:

| | own group | hilltop | far group | slots/hop | far latency |
|---|---|---|---|---|---|
| Election | 98.5% | 82.9% | 72.3% | 6.32 | 4.46 slots |
| **Barrage** | 98.5% | 95.8% | **95.4%** | **1.00** | **1.01 slots** |

- **The latency chain is now exactly linear**: 2 hops 1.0 slots, 3 hops 2.0, 4 hops 3.0.
- **Mouth-to-ear 491 ms → 340 ms** at four hops, recovering 151 ms of budget.
- **`MANET_VOICE_TTL` 4 → 7.** The first hop is direct and free, so seven hops costs six
  slots = 240 ms, giving 460 ms mouth-to-ear against 500. Eight is the edge at exactly 500
  and is not taken. Verified on a forced 14-radio chain at 2.6 km spacing: hop 7 delivers
  83.1% at 240 ms.
- **Twelve hops is now purely a frame-duration question.** At 110 ms — which 22.4 kbps with
  Codec2 2400 buys, both [OQ-0001](../open-questions.md#oq-0001) — one slot per hop gives 12.
- **Dense case improves, but converges more slowly.** Eight groups, 32 radios: worst node
  29.7% → 80.5% and mean 60.5% → 93.2% at steady state. At 1500 slots the worst node reads
  *worse* than the election (4.5% vs 25.9%), because barrage depends on neighbour tables that
  need several beacon intervals to fill. `many_groups` default run length raised 1500 → 6000;
  it had been reporting convergence-phase numbers.
- **More airtime.** Relay transmissions 3496 → 4701 in the hill case, +34%. This lands
  directly on [OQ-0026](../open-questions.md#oq-0026)'s duty-cycle finding and makes it worse.

## The assumption this rests on

**`CONCURRENT_IDENTICAL` is a modelling assumption that is now ON by default and changes every
delivery figure in the project.** It is what the cited literature describes, and it is not
verified on our hardware. Recorded as [OQ-0028](../open-questions.md#oq-0028).

The physical requirement is that copies land within a symbol of one another. At 9600 sym/s a
symbol is 104 µs and a few kilometres of path difference is ~10 µs, so timing is comfortable
with GPS-disciplined slots. **Carrier frequency offset between transmitters is the real
risk** — it beats, and destructive periods must be ridden out by FEC, of which there is 16%.
This is the same dependency as the acquisition preamble figure
([OQ-0024](../open-questions.md#oq-0024)): a disciplined LO, not merely a disciplined clock.

## Reversal trigger

Bench test (OQ-0028) shows two co-slot relays carrying an identical payload do not decode
reliably — either from frequency-offset beating or from timing spread. Then the election
returns, and with it 6.32 slots per hop and a three-to-four-hop product.

## Alternatives rejected

- **Keep the election, shrink the contender set.** Treats the symptom. Even a contender set
  of two costs an expected two slots per hop, halving reach against barrage. And it is
  answering a question the literature review §111 says has no answer: pruning frees no slots.

### What this does not solve, and where the review points next

Concurrent *calls*. Review §103: because we flood, one pipelined call occupies all four slots
in every neighbourhood at once, so ADR-0008's "four concurrent calls when clustered" is true
only single-hop and "collapses to one at the first relay". Barrage does not change that — it
makes the flood faster, not narrower.

The review's escape is **Controlled Barrage Regions** (Halford, Courtade & Turck, MILCOM
2012, §109): source and destination each flood a hop count, every node learns d(s), d(d) and
the shortest-path length, and relays only where d(s)+d(d) equals it — containing the flood to
a corridor instead of the whole network. It needs three hop counts, no link-state database
and no routes to non-neighbours, **and it is what finally uses the dead `hdr.dst` field**
([OQ-0021](../open-questions.md#oq-0021)). That is the next piece of MAC work, and unlike
this one it was scoped correctly the first time.
- **Elect, but only among radios that actually heard the frame.** Correct in principle and
  unimplementable: a radio cannot know which of its neighbours decoded a given burst without
  a round of signalling that costs more slots than the election it replaces.
- **`PROTECT_TALKER_PHASE`** (relays refuse the originator's phase, derived from `src`).
  Measured 82.9% → 95.8% at the hilltop but failed the latency gate at 1062 ms against 500,
  because deferral compounded with the election. Retained in the harness, off, and largely
  made redundant by barrage — worth revisiting only if the election ever returns.
