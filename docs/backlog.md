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

## Phase 0 — remaining, and none of it blocks Phase 1

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
- [ ] **B-10 · `multi_talker` is never called.** The harness function that would have caught
      B-04b exists and nothing invokes it. *Hours.*
- [x] **B-11 · Hardcoded distances, fixed structurally.** ✅ **Done 2026-08-22.** Four
      scenarios had pinned distances in metres against a radio horizon that later moved, and
      every one kept reporting PASS while testing nothing — gate Q1, gate Q5, `hill.py`, and
      `many_groups` (371 m spacing inside a 4416 m horizon: a cluster wearing a chain
      costume). Now: `sim/manet/geometry.py` converts *intent* — one hop, three hops, just
      out of range — into metres against the horizon measured at run time, and
      `make geometry-check` is part of `make all` and rejects bare distance literals in
      scenarios. Non-distances stay named or carry a `geometry-exempt:` reason on the line.
- [ ] **M-03 · Quote ranges as distributions, not points.**
- [ ] **M-05 · Report recovery as time-to-voice, not time-to-tables.** B-06's alarming
      number measured neighbour-table convergence; voice recovers immediately because
      barrage floods rather than routing. The gate's Q5 reports a table figure. *Hours.* FFI give median, 10% and 90% at
      roughly 1:3. We quote single numbers. *Half a day.*

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
- [!] **D-03 · Oscillator: TCXO or MEMS OCXO?** £3 buys 55 min of blackout holdover, £15 buys
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

## If only one thing happens next

**M-01 — re-run at γ = 4.** It is the last Phase 0 item that can change a conclusion, it costs
nothing but CPU, and every reach figure in the project sits on an exponent that the only
comparable published work says is optimistic. Then Phase 0 closes and the three decisions in
Phase 1 are what unblock everything else.
