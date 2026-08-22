# How it works — from pressing the button to voice coming out

This document follows **one burst of speech** from a leader pressing PTT to the sound arriving
in someone else's handset, and explains why each mechanism is there. It is written to be read
start to finish.

**It is also the live reference for the numbers.** The decision log records *decisions*, the
backlog records *defects*, and the open-questions register records *unknowns* — none of them
is organised to tell you how the machine runs, and several of them state figures the code has
since moved past. Where this document and an older document disagree, this one has been
checked against the source. Every figure below is cited to the code, not to prose.

Three labels are used throughout, and they matter more than anything else here:

- **Built** — implemented in the C core, tested, running.
- **Modelled** — exists in the Python simulator only. Real behaviour, no firmware.
- **Unbuilt** — designed and written down, not implemented anywhere.

---

## 0. The shape of the problem

Twelve or so youth-organisation leaders, off-site, spread over a few kilometres of ground that
is usually wooded and often hilly. They need to talk to each other. There is no infrastructure
to lean on and nobody is going to erect a repeater.

The obvious answer — everyone on the same simplex channel — fails on terrain, not on distance.
A hill between two handsets is worth tens of decibels and no amount of power fixes it. So
**every handset is also a relay**: if A cannot hear C directly but both can hear B, B repeats
what it hears and the conversation works.

That single decision creates every hard problem in the rest of this document. If everyone
repeats everything, when does anyone transmit? How does a repeat not collide with the original?
How does the network stop repeating? How does anyone know who can hear whom? The mechanisms
below are the answers, in the order a burst of speech meets them.

---

## 1. Time: slots and frames

**Built.** `core/include/manet/config.h:44,48,162`

Everything rests on chopped-up time. The channel is divided into **40 millisecond slots**, and
**four slots make a 160 ms frame**.

```
|<---------------------------- frame, 160 ms ---------------------------->|
| slot 0, 40 ms | slot 1, 40 ms | slot 2, 40 ms | slot 3, 40 ms |
```

Slots are numbered continuously from a fixed epoch — slot 4,196,392, not "slot 2 of this
frame". The frame is just a view: frame number is the slot number divided by four, and the
position within the frame is the remainder. There is no separate frame counter and no
frame-boundary logic anywhere in the timing code (`core/src/slot.c:5-21`).

That continuous numbering turns out to matter enormously, and section 5 explains why.

**A slot is not transmitted end to end.** The last **3.32 ms** is dead air — the *guard* — so a
transmitter can ramp down cleanly and a relay can flip its receiver into a transmitter without
splattering the neighbouring channel. What is left is a **36.68 ms burst**, which at 19.2 kbps
carries **704 bits** (`config.h:58,163-164`).

Those 704 bits are the entire budget for one hop of one speaker. Everything that follows is
an argument about how to spend them.

---

## 2. Pressing the button

**Mostly modelled.** `sim/manet/world.py:273-289`, `core/src/slot.c:325-338`

When a leader presses PTT, **the handset does not transmit immediately, and it does not listen
first to check whether anyone else is talking.**

Instead it waits for its own **phase** — a fixed one of the four positions in the frame. The
phase is calculated by scrambling the radio's own address through a hash and taking the
remainder modulo four. Then it emits one payload in that position of every frame, for as long
as the button is held.

The important property is that **nothing is negotiated**. No request, no grant, no handshake.
Every other radio can work out where a given talker will speak simply by reading the sender's
address off a frame it already holds. That is why it is a hash and not an election: voice needs
a slot every single frame, and an election in an eleven-radio neighbourhood wins you roughly
one slot in twelve.

**The cost is a 0–160 ms wait before the first burst leaves the antenna** — mean about 80 ms —
set purely by where in the frame the button happened to be pressed.

**The hash is an avalanche mix rather than a simple multiply** for a specific reason: with
consecutively-issued addresses, "times five modulo four is just modulo four", and every odd
address would land on the same two phases (`core/src/slot.c:320-324`).

### The gap you should know about

Four phases and twelve radios. **Several radios permanently share a phase.** If two of them key
up at once, their bursts land on top of each other in the same slot of *every* frame until one
of them stops — a standing collision, not a glancing one. Measured: one stream at 95%, the
other at 0% (`core/include/manet/slot.h:199-207`).

There is no floor control, no busy check and no arbitration for this case. It is
[OQ-0009](open-questions.md), recorded as open, and the named fix is W-04 Controlled Barrage
Regions — confining a call to a corridor instead of flooding the whole network.

Worth knowing: a collision-avoidance function **already exists in the core and is wired to
nothing**. `manet_voice_phase_avoiding()` walks forward from the hashed phase to the first
phase not already carrying another source. It is implemented, unit-tested, and never called
(`core/src/slot.c:340-364`).

**Beacons, unlike voice, do have a channel-access rule** — election, reserved slot, deferral
while voice passes. That asymmetry is deliberate and section 7 explains it.

---

## 3. Turning speech into bits

**Unbuilt.** `docs/decisions/0004-codec2-3200.md`, `config.h:93-100`

There is no vocoder code, no modem and no preamble in this repository. What exists is the
arithmetic — computed at compile time and enforced by a static assertion that refuses to build
a frame which cannot physically hold its own contents.

Codec2 at 3200 bps produces **64 bits every 20 ms**. The radio does not send each one as it
appears; every transmission has to begin with a fixed lump of "wake up and lock on" bits that
costs the same whether it carries one packet or twenty. So the radio **collects eight of them —
512 bits, 160 ms of speech — and sends the lot in one burst per frame.**

That is where the 160 ms frame comes from. It is not arbitrary: it is eight Codec2 frames.

### Where the 704 bits go

| | bits | |
|---|---|---|
| preamble + sync | 56 | finding the burst at all |
| header | 42 | who, from whom, to whom, how many hops left |
| speech | 512 | eight Codec2 frames |
| **error correction** | **94** | **what is left over** |

Two thirds speech, one third machinery.

**A trap worth flagging:** `MANET_SYNC_BITS = 56` is a *time budget in bit-times*, not a count
of bits on the wire. The CC1200 sends preamble and sync as 2-GFSK while the payload is 4-GFSK,
so each preamble bit occupies two payload bit-times. 56 bit-times is **28 actually-transmitted
bits** (`config.h:62-66`). Reading "56-bit preamble" as 56 bits of pattern is wrong by a factor
of two, in the wrong direction.

The preamble matters because it is pure overhead paid on *every* burst at *every* hop. It was
originally budgeted at 154 bits, imported from a NATO report; re-deriving it from the CC1200's
own documentation brought it to 56, and that single correction is most of why the network
reaches seven hops instead of three ([preamble-budget.md](preamble-budget.md)).

**94 bits of FEC is 16% of what it protects.** DMR spends 47%. That gap is real and
[OQ-0002](open-questions.md) is still open — but 47% is *provably unreachable* at 19.2 kbps
with Codec2 3200 at any frame length, so the question is how much protection is enough, not how
to reach parity.

---

## 4. The envelope

**Built.** `core/include/manet/frame.h`, `core/src/frame.c`

Every burst opens with a fixed **42-bit (6-byte) header**, and *everything* the mesh does with
that burst is decided by reading those 42 bits and nothing else. Routing never looks at the
payload — which is what lets voice, text and position share one mechanism.

The header names **three** radios, and the distinction between two of them is the single
easiest thing to get wrong:

| field | meaning |
|---|---|
| `src` | who originally spoke. **Never changed.** |
| `prev` | who transmitted *this copy*. **Rewritten at every hop.** |
| `dst` | who it is for |
| `type` | voice, beacon, text, position… |
| `seq` | a per-sender counter |
| `ttl` | hop budget, counts down |
| `prio` | emergency / voice / signalling / data |

> Duplicate suppression keys on `src`; the forwarding decision keys on `prev`. Confusing the two
> produces a mesh that either loops or refuses to relay. — `frame.h:71-73`

Relaying rewrites exactly two things: `prev`, and the TTL. Nothing else.

Destination *kind* — one radio, a talkgroup, everyone — is not a flag. It is read off which
numeric range the address falls in.

### What is *not* in the header

No timestamp. No slot number. No route. A receiving radio already knows what time it is (from
GPS — see section 8), and works out where to send the frame next from `prev` plus its own
neighbour table, never from anything written in the frame.

That absence is load-bearing, and section 8 is where it bites.

---

## 5. How it travels

**Built (core), with the flood driven by the harness.** `docs/decisions/0011-barrage-relaying.md`

Here is the part that is unlike an ordinary radio network.

When a handset finishes a 40 ms burst, **every other handset that decoded it retransmits the
identical bits in the very next 40 ms slot.** Their neighbours do the same in the slot after
that. The sound spreads outward like a ripple, one slot per hop, with many radios firing the
same waveform at the same instant rather than taking turns.

```
slot n     A ────────► speaks
slot n+1        B C D ────────► all three repeat it, simultaneously
slot n+2              E F G ──► and so on
```

**The relay goes out in slot n+1, not in the next frame.** Because slot numbers are continuous,
a frame boundary is not an obstacle. This is the decision the whole product rests on: waiting
for the next frame would cost 160 ms per hop, and seven hops would be over a second.

### Why simultaneous copies do not destroy each other

The normal assumption in radio is that two transmitters in one slot ruin each other. **This
design assumes the opposite for identical copies**, and it is deliberate: several relays sending
the identical waveform in the identical slot look to a receiver more like multipath than like a
collision. That is how TrellisWare's barrage relay networks and Glossy work.

In the model, `Channel.decode` excludes identical copies from the interference sum, so the flood
never jams itself (`sim/manet/radio.py`). Measured across every density tested: **zero
voice-against-voice collisions.**

**This is the single biggest assumption in the project.** It is flagged in ADR-0011 in capitals
and it is a bench question ([OQ-0028](open-questions.md)), not a settled fact.

**And the model is deliberately conservative about it:** the strongest identical copy is decoded
and the others are *merely excluded from interference*. **No combining gain is claimed**, though
Glossy measures one. The consequence is worth stating plainly, because it drives a lot of
behaviour: **N radios standing together receive exactly what one radio receives.** Six people in
a huddle are one receiver, not six.

### What stops the ripple going round forever

Three things:

1. **The hop budget.** Voice is stamped with TTL 7 and dropped when it expires (`config.h:383`).
   Seven is set by the latency budget, not by the field width (which allows 15).
2. **A short memory.** Each radio remembers the last 32 payloads it has seen, keyed on
   `src`+`seq`, so a payload coming back at it is recognised and never repeated twice.
3. **Coverage pruning** — a radio that concludes it reaches nobody new can stand down.

**Do not describe the flood as "selective".** Pruning is written to fail *open* in every
ambiguous case — an unknown sender relays, an unknown link relays, no advertised coverage
relays — and the frontier threshold is 250 out of 255, so anything short of a near-perfect link
relays anyway. A sweep at 255 (relay always) delivers no better than at 250. **Pruning is doing
very little, by design** (`core/src/mpr.c:229`).

### The older documents will tell you otherwise

Most top-level documents still describe voice as forwarded by **MPR-selected relays** — an
OLSR-derived scheme where each node picks the minimum subset of neighbours needed to reach
everyone two hops out. That was ADR-0003, and **ADR-0011 reversed it for voice.**

MPR selection still runs, and still fills in the link codes advertised in beacons. It no longer
decides who forwards speech. If you have absorbed the "select the minimum subset" story, you
cannot make sense of why a hop costs exactly one slot, or why airtime went *up*, or why beacons
needed reserved slots.

---

## 6. Arriving

**Modelled.** `sim/manet/world.py`

A radio that decodes a voice frame addressed to a group it belongs to plays it out. A radio that
decodes one not for it still *relays* it — that is the whole point of everyone being a relay.

The end-to-end delay, at seven hops:

| | |
|---|---|
| packetisation (collecting 160 ms of speech) | 160 ms |
| waiting for your phase | 0–160 ms, mean 80 |
| seven hops at 40 ms | 260 ms* |
| de-jitter buffer and decode | ~60 ms |
| **mouth to ear** | **~480 ms against a 500 ms budget** |

\* 6.5 slots rather than 6, because voice steps over the reserved signalling slots it meets —
see section 7.

Seven hops is where the **clock** runs out, not where the audio stops being worth listening to.
That distinction matters when reading the atlas: a radio showing red is often perfectly well
connected and simply too many hops away.

---

## 7. Knowing who is there

**Built.** `core/src/neighbour.c`, `core/src/nama.c`

None of the above works unless a radio knows who it can hear. Each handset keeps a **fixed list
of sixteen** neighbours and periodically broadcasts that list. Those broadcasts are **beacons**,
every 33 frames — **5.28 seconds**.

**Hearing someone only proves one direction.** You know a link works both ways only when your
own address comes back inside *their* broadcast list. A one-way link is worse than no link:
relaying through a radio that cannot hear you fails silently and looks like a coverage problem.

### Who transmits a beacon, and when

Beacons go out only in slots the radio has **won in a silent election** (NAMA). Each radio
scrambles its own address together with the slot number to get a priority, does the same
arithmetic for every radio it knows within two hops, and transmits only if its own number is
highest. Everyone computes the same answers from the same inputs, so **nobody has to ask
permission and nobody collides**.

What this replaced was hashing an address into a slot pool with no way to detect a collision:
two radios that hashed alike never heard each other and were invisible to every neighbour in
common, permanently and silently — about a 39% chance across twelve radios.

### The reserved signalling slot

**One slot in eight — 12.5% of airtime — is reserved for signalling, and beacons transmit only
there** (`config.h:350`).

This is recent, and it is the fix for a defect that took a long time to find. Beacons and the
voice flood used to share every slot with nothing arbitrating between them. NAMA elects beacons
against *other beacons*; it cannot see voice, and under barrage relaying there is no voice-free
slot left for it to find. So beacons keyed up over live speech by construction.

The effect scaled with headcount in a way that looked like a mesh defect: more radios means more
beacons, but — because no combining gain is claimed — more radios does *not* mean better
reception. **On the air, six radios standing together are one radio for listening and six radios
for interrupting.** A seven-position chain delivered 92.5% with one radio per position and 77.3%
with six.

The fix has two halves, and both are necessary:

1. Reserve the slot, and put beacons only in it.
2. **Voice steps over the reserved slot rather than dying in it** — `manet_slot_next_voice()`,
   called from `place()` in `core/src/slot.c:99-101`.

The second half is what had been missed. This mechanism had been tried before and written off,
with real measurements showing it cost nine points of delivery. Those measurements were correct;
the conclusion was not. The old attempt reserved the slot but *dropped* any voice arriving during
it, which punched a hole in the relay pipeline. The stepping-over function had been sitting in
the codebase, written and tested, never called.

Result: **99.93% at every density from one to six radios per position**, and the density penalty
gone. See [ADR-0014](decisions/0014-reserved-signalling-slots.md).

### When the table is full

Sixteen entries. When it is full, a newcomer that is not clearly stronger than an existing entry
is **turned away** — the failure mode is refusal of the newcomer, not eviction of an incumbent.
In a dense group, own-group neighbours standing a few metres away are strong, so every refusal
lands on the cross-group links that carry hop progress.

**A caveat the header itself gets wrong:** `neighbour.h:11` says "Only symmetric links are ever
used for relaying." That is no longer true of voice. The relay gate deliberately returns true for
a sender not in the table at all, and for one whose link state is NONE. That was a fix for a real
defect — an evicted sender's frames stopped dead and took one side of a chain from 100% to 49%.
Symmetry still gates MPR selection, the two-hop view and the election. It no longer gates
carrying a broadcast onward.

---

## 8. Agreeing what time it is

**Layer 4 built; layers 1–3 unbuilt.** [ADR-0012](decisions/0012-network-time-authoritative.md)

Everything above assumes every radio agrees what slot it is. So how?

**Nothing on the air ever says what time it is.** Every radio takes its own clock in
microseconds, divides by 40 ms, and calls the answer the slot number. Two radios land in the
same slot only because their clocks already agree — and today the thing making them agree is
**GPS**.

The whole tolerance for that agreement is the guard interval: **3320 µs out of 40,000**, about
8.3% of a slot. To transmit in genuinely the wrong slot a radio would have to be a full 40 ms
out, twelve times the guard budget.

ADR-0012 says this GPS dependence should go away, and the mechanism is elegant: **the flood that
carries voice already carries timing.** Every radio transmits in a slot deterministically related
to the originator's, so *receiving a flood tells you what time it is*. Glossy — the paper this
comes from — is titled "Efficient Network Flooding **and Time Synchronization**" and reports
sub-microsecond error as a free byproduct. The reserved signalling slot helps here too: it
guarantees a regular beacon whether anyone is talking or not.

The ADR sets out four layers. **Only the last — everyone has GPS — is what the code does.** Cold
start with nobody having GPS is `W-02`, and is genuinely hard.

### How much of the protocol actually needs absolute time

Less than you would expect ([OQ-0035](open-questions.md)):

| mechanism | needs | could a newcomer derive it by listening? |
|---|---|---|
| voice phase | its own address; **no clock at all** | yes |
| reserved slot | the slot number **mod 8** | yes — beacons land one in eight |
| relaying, neighbour ageing | differences only | yes |
| **NAMA election** | the **absolute** slot number, hashed | **no** |

**NAMA is the single mechanism forcing absolute agreement.** If it can be made to work on a
repeating pattern instead, cold start gets materially easier — a radio could join by listening.
If it cannot, the wire format has to carry a slot number, and that is a change to price in now
rather than discover later.

---

## 9. How far it actually goes

**Modelled.** `sim/manet/radio.py`, [ADR-0010](decisions/0010-terrain-diffraction.md)

A 5 W handset is +37 dBm at the power amplifier, but that is not what leaves the radio. A
stubby 18 cm helical costs 4 dB against a proper quarter-wave, and a belt-clipped radio against
a torso costs another 4, so **+29 dBm** is what actually radiates. The receiving end pays the
same two penalties in reverse (−8 dB) and can make sense of anything above **−116 dBm**.

Add those up and the path is allowed to eat **137 dB** — and that single number is what every
range figure in the project is derived from (`sim/manet/radio.py`, `LinkBudget`).

The model spends it three ways:

- **Distance.** Signal thins as it spreads.
- **Woodland.** ITU-R P.833 adds up to about 11 dB and then *stops growing*, because the signal
  begins travelling over and around the trees rather than through them.
- **Terrain.** Only when a scenario supplies a height field: knife-edge diffraction over a
  ridge, worth tens of dB. **This is what actually severs links**, and it is the reason the
  product exists.

Run out to where the budget is exhausted:

| | one hop |
|---|---|
| dense woodland | **4.4 km** |
| open moorland | **5.9 km** |

Scenarios place radios at **0.55×** that for a link that dependably works, and **1.6×** for one
that dependably does not (`sim/manet/geometry.py`).

### The assumption that matters most

**The channel is deterministic.** Two radios the same distance apart always get the same answer,
because per-link shadowing is switched off (`Channel.SHADOWING = False`).

Real ground does not work that way. At 2.5 km in woodland, one pair has a thick stand of
conifers and a rise between them and another has a clear run down a valley. That scatter —
**shadowing** — is the part of path loss distance does not predict. The model has it implemented
at σ = 7 dB but disabled. Switched on, woodland range stops being 4.4 km and becomes a
distribution:

| | woodland range |
|---|---|
| nine links in ten manage | 2.2 km |
| half manage | 4.4 km |
| one link in ten manages | 8.8 km |

A four-fold spread between an unlucky link and a lucky one.

> **Note the ordering.** `usable_range_quantiles` returns (p90, median, p10) — the 90% figure is
> the **shortest** distance, because it is the range nine links in ten will manage. Reading it
> the other way inverts the conservative case.

**Every delivery percentage this project quotes is therefore a clean-channel result**, including
the 99.93% above and every card in the atlas. Turning shadowing on is open item **M-06**, and it
will pull those numbers down. Nothing here should be read as a field prediction.

---

## 10. What is actually built

| | state |
|---|---|
| slot timing, pipelining, reserved signalling slot | **built** |
| frame header, pack/unpack, addressing | **built** |
| neighbour table, beacons, symmetry, NAMA election | **built** |
| MPR selection, coverage pruning, duplicate suppression | **built** |
| the scheduler and priority arbitration | **built** |
| the voice phase rule, the flood, PTT | **modelled** — the primitives are in the core, the policy is in the harness |
| propagation, terrain, delivery measurement | **modelled** |
| vocoder, modem, preamble, radio hardware | **unbuilt** |
| network time without GPS | **unbuilt** — designed, ADR-0012 layers 1–3 |
| simultaneous PTT arbitration | **unbuilt** — OQ-0009, no mechanism |
| concurrent calls | **unbuilt** — W-04, "the single highest-value unbuilt item" |

The C core obeys [ADR-0006](decisions/0006-c-core-python-harness.md) throughout: freestanding
C99, no allocation, no floating point, no clock, no globals, and `make arm` reports **0 data,
0 bss**.

---

## 11. Reading the older documents

The decision log is a record of *decisions taken*, and ADRs are not rewritten when the world
moves — they are superseded. That is correct practice, but it means a reader assembling a
picture from them will assemble one two revisions out of date unless they know the chain.

**The four things most likely to mislead you:**

1. **The 60 ms frame / 15 ms slot.** The most-repeated fact in the repository and false for
   several commits. It appears in the engineering brief, four ADRs, the addendum, the literature
   review and several code comments. **The live figures are 160 ms and 40 ms**, pinned by
   `core/tests/test_config.c:23`.
2. **MPR-selected voice relaying.** Described as the relaying rule in every top-level document.
   Reversed for voice by ADR-0011 — see section 5.
3. **Hop count and latency.** Five different answers are live in the documents simultaneously
   (15 hops at 300 ms, 8, 4, 2, 5). **The live answer is seven hops at ~480 ms.**
4. **The frame decision has no accepted ADR.** ADR-0009 set the frame, its own reversal trigger
   fired when the preamble was re-derived, the frame moved — and no superseding ADR was written.
   The only correct statement of the live frame in the whole decision log sits inside ADR-0008,
   which is marked *Superseded*.

Point 4 is a process failure rather than a technical one, and it is what this document exists to
stop happening again. **Where the code and a document disagree, the code wins** — and
`core/include/manet/config.h` is where the code says so.
