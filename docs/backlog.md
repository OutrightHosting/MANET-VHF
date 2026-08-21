# Backlog — what to hit, in order

This is the **work** tracker. [open-questions.md](open-questions.md) is the *research* tracker
— things we do not know. [decisions/](decisions/) is what is settled. This file is what
somebody actually does next.

Ordered so it can be worked top-down. Each item states what "done" looks like, because
several things on this list were previously "done" in a way that turned out not to be.

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

## P0 — The instrument is broken. Nothing below is trustworthy until this is fixed.

- [x] **B-01 · Gate Q1 never goes multi-hop.** ✅ **Fixed.** Stretch is now `DISPERSAL_HOPS × RANGE_M` = 14.8 km, reaching **4 hops** deep, and the criterion fails if depth < 2. `dispersal()` stretches the group to 3000 m
      ([gate.py:48](../sim/scenarios/gate.py)) against a 4416 m woodland range, so every node
      is one hop from every other at every sample. The mobility criterion cannot fail.
      **Done when:** `spread_max` forces ≥ 3 hops at maximum stretch and the test still
      passes — or fails honestly. *Hours.*
- [x] **B-02 · Gate Q5 never partitions.** ✅ **Fixed.** Separation is now `PARTITION_MARGIN × RANGE_M + jitter` = 7.4 km; halves genuinely split to 6/12 each, and the criterion fails if they never lose contact. `SplinterRejoin(separation_m=4000.0)`
      ([gate.py:96](../sim/scenarios/gate.py)) against 4416 m range. The log reports 12/12
      reachable while "apart", and then reports reconvergence from a split that never
      happened. **Done when:** the halves genuinely lose contact and the reported
      reconvergence time is real. *Hours.*
- [x] **B-03 · Re-read every "gate 5/5" claim made since terrain landed.** ✅ **Done.** The claims were unsupported when made but survive the corrected test: 5/5 still, now on criteria that can fail. Two of five
      criteria could not fail. Correct anything downstream that leaned on them. *Hours.*

## P1 — Lead time. Hardware procurement; order early because it just waits.

- [!] **L-02 · Order the bench motherboards.** 4× SMARTRFTRXEBK. Chip-independent, so it does
      not wait on D-01. **Done when:** ordered. *An hour.*
- [!] **L-03 · Order daughtercards.** Blocked on **D-01**. Either 2× CC1120EMK-169 or a fab
      run of CC1200EM_169 rev 1.2.

## P2 — Decisions only the user can make. Everything downstream stalls on these.

- [!] **D-01 · Bench part: CC1120 or fabricate CC1200 at VHF?**
      [ADR-0005 is re-opened](decisions/0005-cc1200-stm32-bench-platform.md) — no CC1200 EVM
      exists in our band. Recommendation is CC1120EMK-169 on blocking performance, which
      matters more in a near-far mesh than anywhere else. **Now complicated by CPM** — see
      [nbwf-lessons.md §3](nbwf-lessons.md).
- [!] **D-02 · Strategy: hop count, or range per hop?**
      [OQ-0030](open-questions.md#oq-0030). FFI use the lowest data rate to cover the most
      ground per hop and need 1–2 relays for 50 km. We have optimised the opposite. Changes
      what Phase 1 measures.
- [!] **D-03 · Oscillator: TCXO or MEMS OCXO?** £3 buys 55 min of blackout holdover, £15 buys
      9.2 h ([OQ-0031](open-questions.md#oq-0031)). Also serves
      [OQ-0024](open-questions.md#oq-0024) and [OQ-0028](open-questions.md#oq-0028) for
      unrelated reasons. Decide with D-01 — same board.

## P3 — Defects in the build

- [ ] **B-04 · `phase_of_addr` collides.** A randomly chosen pair of talkers has a real chance
      of landing on the same voice phase and delivering 0% to everyone — **in either relay
      mode**. Caps concurrent calls independently of [ADR-0011](decisions/0011-barrage-relaying.md)
      and is why ADR-0008's four-call claim fails twice over. **Done when:** phase assignment
      is collision-free across the address space for the supported network size, with a test.
      *Half a day.*
- [ ] **B-05 · Neighbour table overflows at 24+ co-located nodes.** `MANET_MAX_NEIGHBOURS` is
      16; the scale agent had to avoid dense clusters because of it. **Done when:** the
      overflow behaviour is defined and tested — graceful degradation, not silent loss.
      *Half a day.*
- [ ] **B-06 · Barrage reconverges 7.6× slower after a partition.** 182.9 s to restore 90% of
      cross-boundary links against the election's 24.2 s; 6/16 links at +30 s against 16/16.
      **Three minutes of a safety network not working is a product problem, not a metric.**
      **Done when:** understood and either fixed or accepted in writing with a number.
      *Days.*
- [ ] **B-07 · Concurrent-call starvation.** Barrage converts a probabilistic phase collision
      into a permanent one: at n=24 one call reads 95.5% while the other reads 0.0%. A talker
      is starved below 5% network-wide in 14 of 112 chain configurations against 0 of 112 for
      the election. **Fix is Controlled Barrage Regions (W-04)**, not a patch. *Tracked here
      so it is not forgotten while W-04 waits.*
- [ ] **B-08 · Possible regression in single-talker chain delivery.** An agent measured a
      7-chain at `100/61/46/40/36/35/35` where [OQ-0021](open-questions.md#oq-0021) records
      `100/100/99/99/98/97/97`. Either the entry is stale or something broke. **Done when:**
      reproduced and explained. *Hours.*
- [ ] **B-09 · `dst` is inert.** Declared in the header, written on every frame, read by
      nothing. Private and group calls do not exist. *Days — and it is W-04's prerequisite.*
- [ ] **B-10 · `multi_talker` is never called.** The harness function that would have caught
      B-04 and B-07 exists and nothing invokes it. *Hours.*

## P4 — Re-measure. Cheap, and several headline numbers depend on them.

- [ ] **M-01 · Re-run everything at γ = 4.** FFI discarded the model that gave longer ranges
      as exaggerated and used Egli with exponent 4; ours is 2.97, tuned to make 4.8 km come
      out ([OQ-0023](open-questions.md#oq-0023), re-opened). **Every reach figure in the
      project is downstream of this and it costs nothing but CPU.** **Done when:** the trade
      table and the hop claims are restated at both exponents. *A day.*
- [ ] **M-02 · Re-measure network size.** [OQ-0020](open-questions.md#oq-0020)'s answer is
      retracted — measured on a 60 ms frame before TTL existed. Needs B-05 first.
      *Half a day.*
- [ ] **M-03 · Quote ranges as distributions, not points.** FFI give median, 10% and 90% with
      roughly a 1:3 ratio between quantiles. We quote single numbers. *Half a day.*
- [ ] **M-04 · Cost encryption into the slot budget.** FFI carry ~120 bits of crypto IV and
      link PCI per burst. Against our 704-bit slot that takes FEC from 16% to ~4%. It has
      never appeared in `make budget`. *Hours.*

## P5 — Unbuilt design work, roughly in dependency order

- [ ] **W-01 · Network time transfer** — [ADR-0012](decisions/0012-network-time-authoritative.md)
      layer 2. A node without a fix slaves to a neighbour that has one. Turns the GPS
      dependency from hard to soft. Cheap: the beacon already exists. *Days.*
- [ ] **W-02 · Cold start with no GPS anywhere** — ADR-0012 layer 1. Listen, transmit at a
      randomised offset, converge by consensus, no master. **The genuinely hard part.**
      *Weeks.*
- [ ] **W-03 · Network merge tie-break.** Two islands each reach an internally consistent
      time consensus; when they meet one must yield deterministically. Same problem as
      [OQ-0015](open-questions.md#oq-0015) from the other side. *Days.*
- [ ] **W-04 · Controlled Barrage Regions.** The literature review's answer (§109) to
      concurrent calls: flood only within the corridor where d(s)+d(d) equals the path
      length. Fixes **B-07**, reduces airtime (the only remaining lever on
      [OQ-0026](open-questions.md#oq-0026)), and is what finally uses **B-09**'s `dst`.
      *Weeks. The single highest-value unbuilt item.*
- [ ] **W-05 · Late entry** — [OQ-0015](open-questions.md#oq-0015). Signalling half is already
      done; the rest is not. *Days.*
- [ ] **W-06 · Confirmed calls** — [OQ-0016](open-questions.md#oq-0016). Nothing built,
      blocked on B-09. *Weeks.*
- [ ] **W-07 · Topology-control messages.** [OQ-0011](open-questions.md#oq-0011) closed as
      "TC stays" and TC was never implemented. *Days.*
- [ ] **W-08 · Radio silence.** FFI list it as a requirement they should have had. In a
      network where every handset relays, a silent node is a topology change, not a
      preference — and for these users it is more plausible than for a fire team. *Days.*

## P6 — Phase 1 bench, once hardware exists

- [ ] **T-01 · OQ-0028 frequency sweep. Run this first.** PER against injected frequency
      offset with `FOC_EN=0`, two co-slot relays on an identical payload. **Decides seven
      hops or three, and there is no fallback now that the design is body-worn only.** Sweep
      the *number* of co-transmitters too — the mobility agent measured up to **11** identical
      copies in one slot, and CFO risk scales with the crowd.
- [ ] **T-02 · OQ-0001 achievable bit rate**, and CPM against 4FSK
      ([nbwf-lessons.md §3](nbwf-lessons.md)).
- [ ] **T-03 · OQ-0024 preamble tests A–E** ([preamble-budget.md](preamble-budget.md)).
- [ ] **T-04 · OQ-0010 RX→TX turnaround**, including the STM32 SPI path and PA ramp.
- [ ] **T-05 · Always-on power draw.** Now the binding constraint on the 10–12 hour
      requirement, ahead of PA efficiency ([power-budget.md](power-budget.md)). Currently a
      guess.
- [ ] **T-06 · Junction temperature at sustained duty.** Thermal survives the endurance
      correction as a separate problem.

## P7 — Documentation hygiene

- [ ] **H-01 · Rewrite the OQ-0012 entry** — stale in both directions per the Phase 0 audit.
- [ ] **H-02 · Correct OQ-0004's findings** and re-derive the beacon interval for the 160 ms
      frame. 33 frames was chosen against a 60 ms frame and silently changed meaning.
- [ ] **H-03 · Correct OQ-0017's three unsupported statements.**
- [ ] **H-04 · Chase NBWF's fate.** Was automatic relaying ever added to the STANAG? Was NBWF
      fielded? Strongest available evidence either way on
      [OQ-0029](open-questions.md#oq-0029).
- [ ] **H-05 · Contact the OpenMesh Voice Network project.** ARDC-funded, appears to be
      building this exact system on 50 kbps and four TDMA slots. Flagged in the literature
      review and never actioned.

---

## The three that actually matter

Everything above is real, but if only three things happen:

1. **B-01/B-02** — because until the gate can fail, no result means anything.
2. **M-01** — because every reach figure sits on an exponent the only comparable published
   work says is optimistic.
3. **T-01** — because it is the one measurement that decides whether this is a seven-hop
   product or a three-hop one, and body-worn-only removed the fallback.
