# Backlog — what to hit, in order

This is the **work** tracker. [open-questions.md](open-questions.md) is the *research* tracker
— things we do not know. [decisions/](decisions/) is what is settled.

## How this list is ordered, and why it was re-ordered

It was previously grouped by **type** — defects, decisions, documentation. That is why the
work felt like it was darting about: every new document read injected items into a flat list
where a Phase 2 thermal measurement sat next to a Phase 0 simulator defect, and everything
looked equally urgent.

It is now grouped by **phase**, and there is a rule:

> **A finding gets a phase tag when it is written down, and Phase 0 is closed to new work
> unless it blocks Phase 1.** Everything else is parked below the line, in the phase where it
> belongs. Reading a good paper is not a reason to reopen a finished phase.

Status: `[ ]` open · `[~]` in progress · `[x]` done · `[!]` blocked on a decision

## Scope

**Licensing and spectrum assignment are out of scope for this repository.** No licence
applications, timelines, forms or enquiries appear on this list.

The one exception, and it is an engineering one: **if the design drifts outside what would
pass conformity, that is a defect and it goes on this list.** The relevant constraints stay in
the technical docs because they bound the design — 25 kHz occupied bandwidth, EN 300 113-2
adjacent channel power during burst transitions ([ADR-0002](decisions/0002-tdma-slot-pipelining.md)),
ERP limits, and which VHF band the transceiver is characterised for
([OQ-0027](open-questions.md#oq-0027)). Those are inputs to the design, not administration.

---

# PHASE 0 — Simulation. **Criteria met.**

The [brief](engineering-brief.md) sets three, and all three now hold against a gate that can
fail:

| Criterion | Result |
|---|---|
| Protocol converges in simulation with 12 mobile nodes | **100% of samples**, stretched to 15.5 km and 4 hops deep |
| Cluster case produces zero relaying | **0 relays**, behaves as plain simplex |
| 5-hop chain resolves within 300 ms | **160 ms**, 89.8% delivered |

Gate 5/5 · 235,032 checks · freestanding clean · ARM 0 data, 0 bss · 7 hops at 460 ms
mouth-to-ear against a 500 ms budget.

### What the criteria do not test, and where it went

Phase 0's criteria were written before most of what we now know. These are real, and each has
been filed rather than left to make the phase feel unfinished:

- **Concurrent calls do not work.** One flood saturates every slot in every neighbourhood.
  Second talker gets 11%. → **W-04**, with a mechanism now under it (ADR-0013).
- **Barrage reconverges 7.6× slower after a partition** — 182.9 s against 24.2 s at 32 nodes.
  → **B-06**.
- **Range figures rest on an exponent the only comparable published work calls optimistic.**
  → **M-01**.
- **Everything rests on one unverified assumption** about concurrent identical copies. → **T-01**,
  and it is the first thing the bench does.
- **Every criterion is the repeater triangle.** Long hops, terrain doing the blocking, few
  relays. In dense woodland the links shorten, the same ground costs more hops, and the hop
  budget runs out before the group does — twelve leaders over **2 km** of thick cover have
  **12/12 radios connected and 8/12 hearing voice**. Not a bug, and not fixable by raising
  the TTL: seven hops already spends 460 ms of a 500 ms allowance. →
  **[OQ-0032](open-questions.md#oq-0032)**, **M-07**. *The criteria did not catch this
  because they could not — none of them has short hops in it.*

## Phase 0 — remaining, and none of it blocks Phase 1

One item open: **M-06**.

- [x] **M-01 · Re-run everything at γ = 4.** ✅ **Done 2026-08-21 — and the challenge does
      not survive being run.** Transplanting FFI's exponent alone is wrong in both
      directions: Egli has its own intercept and antenna-height term, so `exponent = 4.0` in
      our model gives 568 m where **Egli proper gives 3967 m against our 4416 m — agreement
      within 11%**. The real uncertainty is **2×, and it is fade margin in the link budget**,
      not the exponent: FFI's published 22 km implies ~12 dB they do not itemise, which puts
      our woodland range at 1.9 km rather than 4.4 km. **Protocol conclusions untouched — 7
      hops at every exponent**, per-hop delivery actually better at the pessimistic one. Also
      caught the gate's defect class in `hill.py`, whose geometry was hardcoded and is now
      derived. [OQ-0023](open-questions.md#oq-0023) closed.
- [x] **B-05 · Neighbour table overflowed silently.** ✅ **Fixed 2026-08-22.** The table was
      first-come-first-served with no eviction: once full, a radio could never learn a new
      neighbour however useful, and however stale what it held. The core returned
      `MANET_ERR_BUFFER` honestly and `world.py:291` discarded it.

      **The flat-cluster case hid it** — 30,000 refusals across 48 radios with delivery at
      100%, because everyone hears the talker directly and the table never matters. It only
      bites where density and multi-hop meet, which is `sim/scenarios/neighbour_pressure.py`:

      | radios | worst before | worst after | mean before | mean after |
      |---|---|---|---|---|
      | 24 | 95.0% | 95.0% | 96.3% | 96.3% |
      | 32 | 92.5% | 91.9% | 94.3% | 93.9% |
      | 40 | 89.5% | 90.9% | 93.6% | 94.0% |
      | **48** | **68.2%** | **84.7%** | 84.5% | **91.3%** |

      **Figures superseded by [B-12](#).** The interference-floor fix changed the channel
      model underneath them; re-measured, the same scenarios now read 95.6 / 93.0 / 66.1 /
      84.9% worst at 24 / 32 / 40 / 48 radios. The 40-radio case got *worse* (90.9 → 66.1)
      while everything else improved — worth a look, and not from the eviction policy, since
      it reads the same with that reverted.

      Eviction order: expired first, then asymmetric, then symmetric — never one that
      selected us as its relay while it is still present, since dropping that silently
      breaks its forwarding path. A newcomer must beat the incumbent by
      `MANET_NB_EVICT_MARGIN` or it is refused, which is what stops two similar neighbours
      displacing each other indefinitely. **84.7% is better, not good** — voice wants ~90%,
      and the residue at that density is likely the two-hop table (`MANET_MAX_TWO_HOP`),
      not this one.
- [x] **B-06 · Partition reconvergence under barrage.** ✅ **Investigated and improved
      2026-08-22 — and the headline figure was measuring the wrong thing.**

      **It reproduces, but only at a narrow interface.** Two halves reconnecting across a
      broad front (110 cross-links) had barrage *3× faster* than the election. A chain
      reconnecting through 14 links had it slower, which is the stress agent's topology.

      **Mechanism, measured.** Not fewer beacons sent — scheduling is near-identical (93%
      lost to NAMA either way, 8% fewer sent). It is beacons not *arriving*: barrage puts
      42% more transmissions in the air, so of slots carrying a beacon,

      | | election | barrage |
      |---|---|---|
      | decoded | 26.9% | 21.8% |
      | receiver deaf, own PA keyed | 5.3% | **11.6%** |
      | collided | 6.9% | **11.7%** |

      **Fixed: triggered updates.** A radio whose symmetric neighbour set changes beacons at
      the next slot it wins instead of waiting out the interval. Full reconvergence 70 s →
      40 s.

      **Rejected: excluding voice from the superframe control slot.** `manet_slot_is_control()`
      has existed in the core since the superframe went in, exported and bound and never
      called. Wiring it up cost **nine points of delivery on the 7-hop chain, 84.0% → 74.5%**
      — the originator loses that payload and the relay chain breaks for anything landing
      there — to buy ~24 s off a convergence voice does not wait for. Left wired and off
      (`Simulation.CONTROL_SLOTS`); it becomes worth having if voice ever stops needing
      every slot.

      **And the severity was overstated by the metric.** "7.6× slower" measured neighbour
      *table* convergence. Barrage floods, so it does not need converged tables to deliver:
      measured at the same split, **the far side is receiving voice again immediately on
      restore in both modes**, while the tables are still catching up. Tracked separately as
      **M-05** — quote service recovery, not table recovery.
- [x] **B-08 · Suspected regression in single-talker chain delivery.** ✅ **Investigated
      2026-08-22 — no regression. Three different things were being compared.**

      | | 7-chain, n0 alone |
      |---|---|
      | OQ-0021 recorded | 100 / 100 / 99 / 99 / 98 / 97 / 97 |
      | **Barrage today** | **100 / 97 / 96 / 93 / 90 / 88 / 87** |
      | Election today | 100 / 69 / 51 / 44 / 38 / 37 / 37 |
      | What the agent measured | 100 / 61 / 46 / 40 / 36 / 35 / 35 |

      The agent's row matches **today's election** within noise — they measured the
      pre-[ADR-0011](decisions/0011-barrage-relaying.md) baseline, not a decline. And
      OQ-0021's near-perfect row matches neither, because it was taken before the
      originator-echo defect was fixed, when a talker relayed its own payload back into the
      network and the spurious copies inflated delivery. OQ-0021 corrected in place.
- [x] **B-10 · `multi_talker` was never called.** ✅ **Fixed 2026-08-22 — and it was worse
      than uncalled.** It carried its own copy of `_schedule_voice`, and that copy had never
      gained the dedup registration that stops a talker relaying its own echo, never gained
      a TTL on the PDU, and never gained PTT accounting. Running it would have measured a
      protocol nobody ships. **An unexercised second code path is worse than none — it looks
      like coverage, and it is why B-04b went unnoticed.**

      `Simulation` now takes `talkers=`, so there is one voice path however many people are
      talking, and the duplicate is deleted. Reported as **Q6** in the gate — not a
      criterion, since the brief sets none, but never untested again. It reports
      `speech_through`, so a talker denied the channel cannot hide behind a denominator that
      shrank with it.

      Current reading, 7-node chain, talkers 0 and 3:

      ```
      n0  PTT 100.0%   100  98   0   0   0   0   0   mean 28.3%
      n3  PTT 100.0%     2   2   2 100  97  95  94   mean 56.0%
      ```

      Both get the channel — B-04b's fix holds — and **neither stream crosses the other**,
      which is [OQ-0021](open-questions.md#oq-0021) unchanged. **W-04** is the fix.
- [x] **M-03 · Quote ranges as distributions, not points.** ✅ **Done 2026-08-22.**

      The shadowing figure is **derived, not borrowed**: FFI's published quantiles for mode
      N1 (22.0 km median, 13.1 km at 90%, 36.9 km at 10%, Egli exponent 4) give
      **σ = 7.0 dB from both sides independently** — the check that it is a real parameter
      and not a rounded ratio. `LinkBudget.shadowing_db`, `usable_range_quantiles()`, and
      the gate now prints the horizon as a distribution.

      **And it turned up an argument for the mesh we did not have.** Independent shadowing
      on each hop partly averages out, so reach becomes *more* predictable the further it
      goes:

      | hops | 9 in 10 reach | median | 1 in 10 reach | spread |
      |---|---|---|---|---|
      | 1 | 2.2 km | 4.4 km | 8.7 km | **3.9×** |
      | 2 | 5.7 km | 9.4 km | 15.4 km | 2.7× |
      | 4 | 13.7 km | 19.6 km | 28.2 km | 2.1× |
      | 7 | 26.5 km | 34.9 km | 45.9 km | **1.7×** |

      The mesh does not only extend reach, it makes reach less of a lottery — **a real
      counterweight to [OQ-0030](open-questions.md#oq-0030)'s case for fewer, longer hops**,
      since a small number of long links is exactly the arrangement most exposed to
      shadowing. `sim/scenarios/reach_distribution.py`.

      Reporting only: the channel model stays deterministic, so results remain reproducible.
      Applying shadowing per link in the simulation is a larger change — **M-06**.
- [~] **M-06 · Apply shadowing per link in the channel, not just in reporting.** M-03 gives
      the parameter (σ = 7.0 dB); the channel was deterministic, so every link at a given
      distance behaved identically and marginal links never flickered.

      **The "would need a seed" objection was wrong** and is part of why this sat unactioned.
      The fade is a hash of position — deterministic across fresh instances, symmetric in both
      directions, stable while radios stand still, re-rolled when they move a grid step. No
      seed, no loss of reproducibility.

      **Implemented as two scales**, after measuring that a single grid-quantised term made a
      whole group share one roll of the dice: 100 m grid, groups 60 × 50 m, so sixteen links
      between two groups took **two** distinct fade values and a scale scenario fell from
      32/32 radios to 4/32. Now a shared term (terrain and clutter both radios look through)
      plus a local term (each radio's own surroundings), variances summing to the same 7.0 dB.
      Total severance of a group boundary: **8.2% → 0.3%**, with mean links up unchanged at
      9.4 of 16. Split recorded as [OQ-0036](open-questions.md#oq-0036) — it is assumed, not
      measured.

      **Measured across eight draws of the field over all 33 atlas scenarios.** Aggregate is
      94.4% clean, 97.1% typical, 86.9% on the worst ground — and the averages are worthless,
      because they blend topologies that never fail with ones that fall apart. **19 of 33 hold
      every radio in every draw.** What separates them is redundancy, not distance or hops:
      every dispersed topology held; lines swung between 1/12 and 12/12; and the repeater
      triangle reads a flawless 12/12 clean and collapses to 4/12 on bad ground. Recorded
      against the constants in [geometry.py](../sim/manet/geometry.py) — `DEPENDABLE` fails one
      link in seven under shadowing and `SEVERED` carries traffic one time in five.

      **Both models now ship** and the atlas has a toggle, so the choice no longer has to be
      made to see the results. What remains open is the *default* for the simulator and the
      gates:

      - clean stays the default for development and regression — B-15 was only findable
        because a deterministic channel makes an exact difference visible, and six hypotheses
        were eliminated that way;
      - nothing quoted anywhere should come from a single draw. Publishing one nearly put a
        wrong figure into the atlas, twice.
      - **the gates need a decision that is not mine:** they are pass/fail against a
        deterministic channel. Under shadowing, does Q1 pass on the median draw or the worst?
        Worst-of-N is the right answer for a safety system and it will fail gates that pass
        today. Q5 additionally needs `SEVERED` re-derived at ~3.0x or its partition
        precondition stops holding.
- [x] **M-07 · Gate the dense-cover case.** ✅ **Done 2026-08-22.** Gate **Q7** — twelve
      leaders in cover with a 350 m horizon, sweeping the spread and reporting where the
      group splits. Currently: holds together to **1.5 km**, splits at 2 km with 12/12
      connected and 8/12 hearing. Reported, not gated — the brief sets no criterion for it,
      and inventing one would be picking a number rather than measuring one.
- [x] **M-05 · Report recovery as time-to-voice, not time-to-tables.** ✅ **Done 2026-08-22.**
      Q5 now tracks whether the far half is *receiving voice*, and leads with that figure.
      It also independently confirms the partition — the far half reads 0/6 while apart,
      which is a second check on the precondition B-02 added. At twelve nodes both are 5 s;
      the two diverge at scale, which is what made B-06 read as "7.6× slower" when the
      service had already recovered.

- [x] **B-12 · Half of all payloads died at the talker's first hop.** ✅ **Fixed 2026-08-22 —
      a simulator defect, not a protocol one.** `Channel.decode` summed interference over
      every transmission in the network including ones far below the demodulator's floor:
      a median of **18 per slot** at 100 radios, contributing a median of **100%** of the
      interference power. A receiver was being jammed entirely by signals it could not hear.
      Chains never showed it because they have one or two transmitters.
      **200 radios went from 50.7% mean and 1/200 usable to 99.7% and 200/200**, and the
      usable hop depth went from 4 back to the full 7. [OQ-0033](open-questions.md#oq-0033).

- [x] **B-13 · Relaying was asymmetric either side of a centre talker.** ✅ **Fixed
      2026-08-22, third attempt.** `manet_mpr_should_relay`'s **first** test was "do I know
      this sender?", and on a miss it returned false and the frame stopped dead. Three lines
      below, the same function already relays when the sender's coverage is unknown —
      because that is exactly when pruning is impossible. The same question was being
      answered both ways. **An unknown sender now relays.**

      It is not a rare case: the table holds 16 and a radio in a moderately dense group has
      more neighbours than that, so a perfectly good sender gets evicted and every frame it
      carries stops at whoever dropped it. Here a single group-3 radio had been evicted from
      the group-4 tables on a **−108 dBm link** — 8 dB clear of sensitivity.

      ```
      before   94  95 100 100 100   2   1   1
      after    94  94  99 100  99  95  89  89
      ```

      **It also resolved the unexplained regression noted against B-05** — the 40-radio
      pressure case, which had dropped to 66.1% after B-12, is now **96.0%**. Same root
      cause, and the reason it looked like a B-12 side effect is that B-12 raised delivery
      enough for the eviction to start mattering.

      | radios | before | after |
      |---|---|---|
      | 32 | 93.0% | 93.7% |
      | 40 | **66.1%** | **96.0%** |
      | 48 | 84.9% | **93.2%** |

      Two earlier attempts failed: per-radio voice phase (ruled out by measurement) and
      protecting frontier neighbours from eviction (implemented, tripled the relay count,
      moved delivery not at all, reverted). The step that worked was shrinking the case
      until the asymmetry vanished, which isolated it to *which* radio in the middle group
      was speaking.

- [x] **B-15 · Per-hop loss scales with how many radios stand at each position.**
      ✅ Fixed 2026-08-22 — [ADR-0014](decisions/0014-reserved-signalling-slots.md).
      **Beacons were jamming voice, and beacons scale with headcount while voice reception
      does not.** Every decode failure at every density involved a beacon; there were zero
      voice-against-voice collisions anywhere, so barrage combining was never at fault.
      Six radios standing together are one radio for voice and six radios for beacons —
      which is also why within-group spread measured exactly zero.

      | radios per position | before | after |
      |---|---|---|
      | 1 | 92.54% | **99.93%** |
      | 2 | 87.21% | **99.93%** |
      | 3 | 83.41% | **99.93%** |
      | 4 | 80.12% | **99.93%** |
      | 5 | 79.39% | **99.93%** |
      | 6 | 77.27% | **99.93%** |

      Six hypotheses were measured and eliminated before the cause appeared: reception,
      within-group variation, coverage suppression, beacon slot overhead, the neighbour
      table cap, and barrage combining gain. The last of those survived one round and was
      killed by a reductio — a meaningless constant 3 dB flattened the gradient *better*
      than the proposed mechanism, which proved the test was measuring link margin.

- [x] **B-16 · Beacons cost delivery even when the mesh is otherwise ideal.**
      ✅ Fixed 2026-08-22 by the same change — [ADR-0014](decisions/0014-reserved-signalling-slots.md).
      Same root cause as B-15, not a separate surcharge: seven radios in a line lost 1.24
      points per hop purely to beaconing, and now lose 0.01. The fix was not fewer beacons
      but somewhere for them to go — the earlier finding that a wider interval helped both
      densities while *widening* the gap was the clue that the interval was never the
      variable.

- [x] **B-17 · Concurrent relays did not transmit the same bits, and the simulator credited
      them as if they did.** ✅ **Fixed 2026-08-22.** `manet_sched_relay` stamped the relaying
      radio's address into every frame, so barrage copies were never identical — 8 header bits
      plus the 94 FEC bits computed over them, ~102 of 704 on-air bits. `_payload_key()` keyed
      on `(src, seq, type)` and called the relay irrelevant, so the model credited combining
      the wire format forbade.

      **The rule:** a frame may only be modified in a way that every relay at the same distance
      modifies identically. TTL obeys it — under slot pipelining a hop *is* a slot, verified
      over ~9400 slot-payload observations with zero mixed-TTL slots. `prev` cannot, because it
      encodes who you are rather than where you are.

      Voice now leaves `prev` as the origin set it; signalling keeps it and is the discovery
      mechanism. Three dependants went with it and all three were already measured as costing
      nothing: neighbour discovery from voice frames, coverage suppression (B-15: no effect at
      any density), and the `should_relay` gate (OQ-0011: relay-always delivered no better).

      | atlas, all 33 scenarios | radios in the conversation |
      |---|---|
      | the old optimistic figure | 775/800 · 96.9% |
      | honest key, `prev` still on voice | **483/800 · 60.4%** |
      | honest key, `prev` off voice | **775/800 · 96.9%** |

      **Every scenario returns to its previous figure — 0 of 33 differ.** The delivery numbers
      published before this were right; the mechanism underneath them was not, and would not
      have survived contact with hardware. `test_concurrent_relays_are_bit_identical` now
      compares the packed wire bytes of two radios relaying one payload in one slot.
      [OQ-0038](open-questions.md#oq-0038) records the six undefined padding bits it found.


## Phase 0 — done

- [x] **B-01 · Gate Q1 never went multi-hop.** Stretch derived from measured range: 15.5 km,
      4 hops deep, fails below 3. Verified to fail at the old 3000 m.
- [x] **B-02 · Gate Q5 never partitioned.** Separation derived: 7.4 km, halves split 6/12
      each way, fails if contact never breaks. Verified to fail at the old 4000 m.
- [x] **B-03 · Re-read every "gate 5/5" claim.** Unearned when made; true against the
      corrected gate.
- [x] **B-04 · Voice phase into the C core**, where it belonged — it was harness-only, so the
      most fundamental MAC decision in the system was not built. The original done-test was
      unachievable (pigeonhole); the achievable one is that concurrent talkers can be
      separated up to the structural limit.
- [x] **B-04b · Barrage muted its own talkers.** The `BARRAGE_RELAY` branch returned before
      the `busy` guard, so a radio relayed another stream in its own origination phase every
      frame. PTT success 9.8% → 100%.
- [x] **B-04c · The delivery metric divided by payloads that reached the air.** `ptt_success`
      and `speech_through` added; the gate now prints PTT beside delivery. Headline
      single-talker figures were checked and are unaffected.

---

# PHASE 1 — Bench RF. Needs hardware.

## Decisions required before ordering

- [!] **D-01 · Bench part: CC1120, or fabricate CC1200 at VHF?**
      [ADR-0005 is re-opened](decisions/0005-cc1200-stm32-bench-platform.md) — no CC1200 EVM
      exists in our band. Recommendation is CC1120EMK-169 on blocking performance, which
      matters more in a near-far mesh than anywhere else. Complicated by CPM — see
      [nbwf-lessons.md §3](nbwf-lessons.md).
- [!] **D-02 · Strategy: hop count, or range per hop?**
      [OQ-0030](open-questions.md#oq-0030). FFI use the lowest data rate to cover the most
      ground per hop and need 1–2 relays for 50 km. Changes what Phase 1 measures.
- [!] **D-03 · Oscillator: TCXO or MEMS OCXO?** **Cannot be decided on holdover alone — see
      T-01.** Both options sit inside the 0–30 Hz window where a beat null spans the whole
      burst, so neither escapes the concurrent-transmission hazard and buying tighter makes it
      worse, not better. £3 buys 55 min of blackout holdover, £15 buys
      9.2 h ([OQ-0031](open-questions.md#oq-0031)). Also serves
      [OQ-0024](open-questions.md#oq-0024) and [OQ-0028](open-questions.md#oq-0028). Same
      board as D-01, so decide together.

## Procurement

- [!] **L-02 · 4× SMARTRFTRXEBK motherboards.** Chip-independent, so it does not wait on D-01.
- [!] **L-03 · Daughtercards.** Blocked on D-01.

## Bench measurements, in order

- [ ] **T-01 · OQ-0028 frequency sweep. First, before anything else.** PER against injected
      frequency offset, `FOC_EN=0`, two co-slot relays on an identical payload. **Decides
      seven hops or three, and body-worn-only removed the fallback.** Sweep the *number* of
      co-transmitters too — up to 11 identical copies were measured in one slot, and the risk
      scales with the crowd, not with two.

      **REWRITTEN 2026-08-22. The sweep as originally specified would have missed the
      failure.** It said 0 to ±1 kHz, linear, and judged success on PER staying flat to
      ±200 Hz. The danger zone is **0–30 Hz** and a linear sweep to 1 kHz steps over it.

      Beat frequency equals carrier frequency offset exactly, and the burst is 36.68 ms. The
      beat period therefore equals or exceeds the whole burst when **|Δf| ≤ 27.3 Hz — 0.176
      ppm at 155 MHz.** Below that a null does not flicker past, it sits on the frame:

      | reference | offset between two radios | beats per burst |
      |---|---|---|
      | GPS-disciplined, 1 ppb | **0.15 Hz** | one null, frozen |
      | good TCXO, 0.1 ppm | **15.5 Hz** | under one |
      | plain crystal, 2 ppm | 310 Hz | ~11, averages out |

      **The better the oscillator, the deeper into the hazard.** Glossy and BlueFlood get
      intra-packet time diversity free from sloppy references; disciplining the LO removes it,
      and TrellisWare dither on purpose. So the sweep must be **logarithmic and concentrated
      low: 0, 3, 6, 12, 25, 50, 100, 250, 500, 1000 Hz.**

      Add two measurements the original did not have:
      - **bit-error position histogram across the burst**, with CRC filtering disabled. A
        histogram periodic at the programmed Δf is direct observation of beating, and says the
        remedy is an interleaver rather than a better reference. Check first whether the packet
        engine will hand up CRC-failed payloads; if not, this needs the I/Q tap.
      - **error rate per symbol level.** The published treatments model BFSK explicitly "for
        simplicity". With 4-GFSK the inner symbols sit one deviation step apart rather than
        two, so a null should corrupt inner before outer. No published treatment of concurrent
        transmission on 4-level FSK was found — a genuine gap, specific to our modulation, and
        free to measure here.

      **This changes [D-03](#decisions-required-before-ordering).** Both oscillator options sit
      inside the danger zone, so that decision cannot be made on holdover alone.
- [ ] **T-02 · OQ-0001 achievable bit rate**, and CPM against 4FSK
      ([nbwf-lessons.md §3](nbwf-lessons.md)).
- [ ] **T-03 · OQ-0024 preamble tests A–E** ([preamble-budget.md](preamble-budget.md)).
- [ ] **T-04 · OQ-0010 RX→TX turnaround**, including the STM32 SPI path and PA ramp.
- [ ] **T-05 · Always-on power draw.** Now the binding constraint on the 10–12 hour
      requirement, ahead of PA efficiency ([power-budget.md](power-budget.md)).

## Protocol work that changes the wire format — do it here, not in Phase 0

- [ ] **W-06b · Add the sender's previous hop to the header, and build gradients.**
      8 bits, FEC 16% → 14.6%. [ADR-0013](decisions/0013-what-we-take-from-vine.md).
      *Prerequisite for W-04.*
- [ ] **W-04 · Controlled Barrage Regions.** Confines a call to a corridor instead of flooding
      the network. Fixes concurrent calls, is the only remaining lever on airtime
      ([OQ-0026](open-questions.md#oq-0026)), and finally uses **B-09**'s inert `dst`.
      *The single highest-value unbuilt item.*
- [ ] **B-09 · `dst` is inert.** Declared, written on every frame, read by nothing.
- [ ] **M-04 · Cost encryption into the slot budget.** ~120 bits of crypto IV and link PCI
      would take FEC from 16% to ~4%, and has never appeared in `make budget`.

---

# PHASE 2 and later — parked

- [ ] **W-01 · Network time transfer** — [ADR-0012](decisions/0012-network-time-authoritative.md)
      layer 2. Turns the GPS dependency from hard to soft.
- [ ] **W-02 · Cold start with no GPS anywhere** — ADR-0012 layer 1. The genuinely hard part.
      Narrowed by [OQ-0035](open-questions.md#oq-0035): voice phase needs no clock and the
      signalling reservation needs only the slot number *mod 8*, both derivable by listening.
      **NAMA is the only consumer that needs the absolute count.** Settle whether its context
      can be modular before assuming the wire format must carry a timestamp.
- [ ] **W-03 · Network merge tie-break.** Two islands, two internally consistent clocks.
- [ ] **W-05 · Late entry** — [OQ-0015](open-questions.md#oq-0015). Signalling half done.
- [ ] **W-06 · Confirmed calls** — [OQ-0016](open-questions.md#oq-0016). Blocked on B-09.
- [ ] **W-07 · Topology-control messages.** [OQ-0011](open-questions.md#oq-0011) closed as "TC
      stays" and TC was never implemented.
- [ ] **W-08 · Radio silence.** In a network where every handset relays, a silent node is a
      topology change, not a preference.
- [ ] **M-02 · Re-measure network size** ([OQ-0020](open-questions.md#oq-0020) retracted).
      Needs B-05 first.
- [ ] **T-06 · Junction temperature at sustained duty.**
- [ ] **P-01 · 5 W PA chain** ([OQ-0025](open-questions.md#oq-0025)) and the duty-cycle
      consequences ([OQ-0026](open-questions.md#oq-0026)).

---

# Housekeeping — do when passing, never as a priority

- [ ] **H-07 · The 90% threshold reads as pass/fail and it is not.** "Four groups along a
      valley" reports 8/16 in the conversation with a **worst node of 89.3%** — seven tenths
      of a point under the line, presented as broken speech. The threshold is a hard line
      through a soft boundary and the cards make it look categorical. **Done when:** the
      atlas shows the distribution rather than a binary, or the flag names the actual worst
      figure so 89.3% is not confused with 40%.
- [ ] **H-08 · The documentation set contradicts the code in 86 places.** Found by audit while
      writing [how-it-works.md](how-it-works.md). Fixed already: the stale comments inside
      `config.h` (beacon interval, superframe reservation rate, hop latency), the
      `CONCURRENT_IDENTICAL` comment that said "off by default" above a flag that is on, two
      comments introduced with ADR-0014 that said one slot in four, the ADR index's wrong
      ADR-0009 row and missing ADR-0014 row, three broken ADR links, and warning banners on
      `engineering-brief.md`.

      **Still outstanding**, and left deliberately because ADRs are historical records that get
      annotated rather than rewritten: the 60 ms / 15 ms frame figures throughout ADR-0002,
      0004, 0007, 0008, 0009, the addendum and the literature review; the MPR-relays-voice
      description in ADR-0003, the addendum and feature-set; five mutually inconsistent hop-count
      answers across the open-questions register; and OQ-0002's sweep table, which does not
      contain the configuration that was actually built.

      **The root cause is a process failure, not a typo.** ADR-0009's reversal trigger fired when
      the preamble was re-derived, the frame moved, and no superseding ADR was written — so the
      only correct statement of the live frame in `docs/decisions/` sits inside ADR-0008, which
      is marked *Superseded*. The decision log's own promise is that a fired trigger produces a
      successor. **Writing that missing ADR is the first task here.**

- [ ] **H-01 · Rewrite the OQ-0012 entry** — stale in both directions per the Phase 0 audit.
- [ ] **H-02 · Correct OQ-0004's findings** and re-derive the beacon interval for the 160 ms
      frame. 33 frames was chosen against a 60 ms frame and silently changed meaning.
- [ ] **H-03 · Correct OQ-0017's three unsupported statements.**
- [~] **H-04 · NBWF's fate.** Partly answered: VINE cites *NATO STANAG 5631/AComP-5631,
      Ratification Draft, Edition 1, 2015*, so NBWF reached a ratification-draft STANAG.
      Whether automatic relaying was added is still open.
- [ ] **H-05 · Contact the OpenMesh Voice Network project.** ARDC-funded, appears to be
      building this exact system on 50 kbps and four TDMA slots.
- [ ] **H-06 · Re-check goTenna Pro X2 against the brief's own rejection.** The brief rejects
      it because it "carries no voice"; they demonstrated real-time voice in Oct 2023.
      **If a body-worn product already does this, that is material to whether to keep building
      one.** Somebody's few hours against months of ours.

---

## Phase 0 is closed

All six criteria met, and the four things they could not see are recorded rather than
buried: concurrent calls (**W-04**), the disputed exponent (**M-01**, resolved), the
unverified concurrency assumption (**T-01**), and short-hop coverage
(**[OQ-0032](open-questions.md#oq-0032)**). The gate reports seven questions, two of them —
concurrent talkers and dense cover — precisely because nothing required them and both hid a
failure.

**Next is Phase 1, and it starts with three decisions that are not mine to make:** bench
part, hop-count-versus-range strategy, oscillator. Everything in the bench queue stalls on
them.

<details><summary>superseded — the Phase 0 closing note</summary>

**M-01 — re-run at γ = 4.** It is the last Phase 0 item that can change a conclusion, it costs
nothing but CPU, and every reach figure in the project sits on an exponent that the only
comparable published work says is optimistic. Then Phase 0 closes and the three decisions in
Phase 1 are what unblock everything else.

</details>
