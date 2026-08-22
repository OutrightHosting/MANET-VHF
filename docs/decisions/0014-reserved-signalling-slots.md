# ADR-0014 — Signalling gets reserved slots, and voice steps over them

**Status:** accepted, 2026-08-22
**Supersedes:** the "provided but deliberately not used" disposition of
`manet_slot_is_control()` recorded in [slot.h](../../core/include/manet/slot.h) and the
`CONTROL_SLOTS = False` decision in the simulator.
**Related:** [ADR-0002](0002-slot-pipelining.md) (one hop per slot),
[ADR-0011](0011-barrage-relaying.md) (barrage relaying), B-15 and B-16 in
[backlog.md](../backlog.md), [nbwf-lessons.md](../nbwf-lessons.md) §5.

## The decision

One slot in eight is reserved for signalling. Beacons transmit only there. Voice never
transmits there and **steps over** the slot rather than being dropped in it.

`MANET_SIGNAL_SLOT_PERIOD` is 8 — 12.5% of airtime.

## What was wrong

Beacons and the barrage voice flood shared every slot, and nothing arbitrated between them.

NAMA (Bao & Garcia-Luna-Aceves, MobiCom 2001) elects beacons against *other beacons* within
two hops, which it does correctly and for free. It cannot see voice. Under barrage relaying
(ADR-0011) voice transmits in **every** slot in the flood's footprint, so after the election
there is no voice-free slot left for a beacon to find. The `QUIET_FRAMES` hold was the last
line of defence and `MAX_DEFERRALS = 6` turned it into a delay rather than a guard: the
radio waited six frames, found the channel still busy, and keyed up over live voice by
construction.

Measured across a seven-position chain, every decode failure at every density involved a
beacon, and there were **no voice-against-voice collisions at all**:

| radios per position | decode failures | involve a beacon | voice vs voice |
|---|---|---|---|
| 1 | 115 | 100.0% | 0 |
| 2 | 316 | 100.0% | 0 |
| 3 | 689 | 100.0% | 0 |
| 4 | 1051 | 100.0% | 0 |
| 5 | 1600 | 100.0% | 0 |
| 6 | 1940 | 100.0% | 0 |

Barrage combining works. The flood never jams itself. What jammed it was signalling.

### Why it got worse with density

Two facts multiply. Beacon transmissions scale with headcount — 9,386 in the air at one
radio per position, 43,929 at six. But `Channel.decode` decodes the strongest identical copy
and merely excludes the others from interference; it claims no combining gain, so six
co-located radios receive exactly what one receives.

**On the air, six radios standing together are one radio for voice and six radios for
beacons.** That asymmetry was the entire density penalty, and it is also why within-group
delivery spread was measured at exactly zero: co-located radios are not independent
receivers, they are one receiver.

At one radio per position a beacon collides with voice the beaconer itself was carrying, and
that radio's own guards apply. At six, the beaconing radio is a *different*, co-located
radio whose guards honestly report "I have no voice queued" while it keys up over its five
groupmates. Density decouples the guard from the harm.

## Why the reservation was previously rejected, and why that was wrong

`manet_slot_is_control()` has existed since the superframe went in, exported through the
bridge, bound in Python, and never called. The header carried a measured table showing the
reservation costing nine points of delivery, and concluded the mechanism was not worth its
airtime.

The numbers were right. The conclusion was not. **The cost was not the reservation — it was
dropping the voice payload that landed in the reserved slot.** That punched a hole in the
relay pipeline, and the delay cascaded into the payload behind it. `manet_slot_next_voice()`
sat beside `manet_slot_is_control()` in the same file, also never called, and is exactly the
missing piece.

The step-over now lives in `place()` in [slot.c](../../core/src/slot.c), so origination and
relaying both get it, and any future caller does too. Signalling priority is exempt — the
slot exists so beacons can use it.

There is a related trap worth recording. Any period that is a multiple of
`MANET_SLOTS_PER_FRAME` always lands on the same voice phase, so with drop-the-payload
behaviour one phase's talkers would lose every origination while the other three lost none —
a silent, address-dependent failure. Stepping over removes the hazard rather than dodging
it, which is the second reason the step-over is not optional.

## Results

Seven positions in a line, varying only how many radios stand at each:

| radios per position | before | after |
|---|---|---|
| 1 | 92.54% | **99.93%** |
| 2 | 87.21% | **99.93%** |
| 3 | 83.41% | **99.93%** |
| 4 | 80.12% | **99.93%** |
| 5 | 79.39% | **99.93%** |
| 6 | 77.27% | **99.93%** |

Per-hop loss goes from 1.24–3.79 points to 0.01 at every density. Originations are unchanged
at 1,368 throughout — the reservation costs no codec frames, because voice steps over rather
than skipping.

Gate results, against the same suite:

| | before | after |
|---|---|---|
| Q1 cluster, min delivery | 1.00 | 1.00 |
| Q5 partition, voice back | 5.44 s | 5.44 s |
| dispersal (mobility), min delivery | 0.966 | **1.000** |
| dispersal, converged fraction | 1.00 | 1.00 |
| multi-talker, worst stream | 0.2839 | 0.2857 |

Mobility improves. Nothing regresses.

## What it costs

**Latency.** Voice steps over each reserved slot it meets, so a chain arrives that many
slots later. Seven hops goes from 6 slots to 6.5 — 240 ms to 260 ms of slot time, and with
ADR-0011's 220 ms of non-slot overhead, **480 ms mouth-to-ear against the 500 ms budget**.

The period was chosen by that budget, not by delivery. Delivery is 99.93% at every period
from 4 to 8, so buying more signalling airtime buys nothing:

| period | airtime | 7 hops | mouth-to-ear | links known (42 radios) |
|---|---|---|---|---|
| 4 | 25.0% | 8 slots | 540 ms ✗ | 93.8% |
| 5 | 20.0% | 8 slots | 524 ms ✗ | 84.3% |
| 6 | 16.7% | 7 slots | 513 ms ✗ | 84.4% |
| 7 | 14.3% | 7 slots | 506 ms ✗ | 61.1% |
| **8** | **12.5%** | **6 slots** | **480 ms ✓** | 63.7% |

Only 8 fits, and it is also the cheapest in airtime. Being frame-aligned is what makes it
both faster and better-informed than 7: a coprime period spreads the cost across all voice
phases but crosses more reserved slots per hop.

**Table convergence at scale.** 63.7% of links known at 42 radios, against 85.2% before. It
is not the trade it looks like — at the twelve-radio design target the mobility gate is
unaffected (dispersal converges 1.00 with delivery 1.00 at period 4 and at period 8), and
delivery at 42 radios is 99.93% either way. It is recorded because it is the NBWF scaling law
showing up in measurement, and it will matter if the network size target ever grows.

Shortening the beacon interval does **not** substitute: it plateaus near 65% at period 8.
The binding constraint is the supply of reserved slots to elect within, not how often a radio
wants to speak.

**Firmware.** Nothing. No new state, no arrays, no allocation. `make arm` reports
4207 text / **0 data / 0 bss** (4187 before — 20 bytes of text). `make freestanding` is
unchanged. ADR-0006 holds throughout.

## The architectural point

ADR-0011's own header says it: *"Supersedes the NAMA election for voice relays only; NAMA
still owns beacons."* That sentence was the defect.

Barrage correctly removed the election from voice, because identical concurrent copies
combine — and the measurements confirm it, with zero voice-against-voice collisions at every
density. But the consequence is that voice occupies every slot in its footprint, and a
two-hop election among beacons cannot find a gap that no longer exists.

A data-mesh heuristic — periodic link-state beaconing on the shared voice channel, arbitrated
by an election that only knows about beacons — was fighting the barrage premise. **The
heuristic gives way.** Barrage is load-bearing: it is what buys one hop per slot instead of
6.32, and it is measurably working.

[nbwf-lessons.md](../nbwf-lessons.md) §5 already contained this answer and had not been acted
on. NBWF's frame has a *fixed* category of slots precisely so signalling is never pre-empted
by voice, and the recommendation recorded there is one additional slot per relay. That is the
same scaling law: signalling capacity must grow with the number of radios that have to
advertise, not stay pinned at one slot per superframe.

## What this does not settle

`Channel.SHADOWING` is `False`, so in these scenarios a link is a deterministic function of
distance and collision is the only loss mechanism available. That is why removing beacon
interference reaches 99.93% rather than some lower plateau. **The 99.93% figure is a
clean-channel result and is not a field prediction.** It does not weaken the causal finding —
what is being explained is the density gradient, and the gradient goes to zero — but the
absolute numbers must be re-measured once M-06 lands.

Whether real Glossy combining gain exists on our hardware is OQ-0028's bench question. Until
it is answered, density buys only redundancy against a jammed groupmate, which is what
appeared the instant the jamming was removed.
