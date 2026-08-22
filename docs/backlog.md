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

- [x] **B-01 · Gate Q1 never goes multi-hop.** ✅ **Fixed 2026-08-21.** `dispersal()` stretched
      the group to 3000 m against a 4416 m woodland range, so every node was one hop from
      every other at every sample and the mobility criterion could not fail. Stretch is now
      derived — `DISPERSAL_HOPS × RANGE_M` = 15.5 km, 3.5× the horizon — reaching **4 hops**
      deep, and the criterion **fails below 3**. Verified to fail at the old 3000 m.
- [x] **B-02 · Gate Q5 never partitions.** ✅ **Fixed 2026-08-21.**
      `SplinterRejoin(separation_m=4000.0)` against a 4416 m range, with ±150 m of cluster
      jitter putting the closest cross-boundary pair at 3700 m. The halves never lost
      contact — the trace printed 12/12 reachable while "apart" — and the runner then
      reported a reconvergence time for a split that never happened. Separation is now
      derived: `PARTITION_MARGIN × RANGE_M + jitter` = 7.4 km, 1.7× the horizon. The halves
      split to **6/12 each way**, heal in 5.4 s to 12/12, and the criterion **fails if they
      never lose contact**. Verified to fail at the old 4000 m.
- [x] **B-03 · Re-read every "gate 5/5" claim made since terrain landed.** ✅ **Done
      2026-08-21.** Two of five criteria could not fail, so every 5/5 since terrain landed
      was unsupported at the time. Re-run against the corrected gate: **still 5/5**, now on
      criteria that can fail. The claims were unearned when made and happen to be true.

**P0 is clear.** The gate tests what it says it tests. Everything below is measured against
an instrument that can now report a failure.

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

- [x] **B-04 · `phase_of_addr` collides.** ✅ **Fixed 2026-08-21, and the done-test as
      originally written was unachievable.** It asked for phase assignment "collision-free
      across the address space", which is arithmetically impossible: there are
      `MANET_SLOTS_PER_FRAME` phases and more radios than that, so ~20–23% of talker pairs
      must share a phase by pigeonhole and no hash changes it. The achievable requirement is
      narrower — **no two radios talking at the same time may share a phase**, of which there
      are at most 4. Delivered:
      - `manet_voice_phase()` and `manet_voice_phase_avoiding()` in **the C core**, where
        they belong. The whole mechanism was harness-only, which by
        [ADR-0006](decisions/0006-c-core-python-harness.md)'s rule meant the most
        fundamental MAC decision in the system — when a talker transmits — was not built.
      - Collision avoidance walks forward deterministically, so two radios resolving the
        same collision from the same information agree without exchanging anything.
      - Saturation is reported rather than hidden (`manet_voice_phase_free`).
      - 803 new checks.
- [x] **B-04b · Barrage muted its own talker.** ✅ **Fixed 2026-08-21.** Found by the
      adversarial stress test, not by me. The `BARRAGE_RELAY` branch returned before
      consulting the `busy` guard that `world.py` already documented, so a radio adjacent to
      another talker's flood relayed that stream **in exactly its own origination phase,
      every frame**, and the scheduler then refused its own voice. Not degradation —
      **lockout**: 9.8% PTT success on an 18-radio ridge, and at 48 radios the verifier
      measured a talker keying up **0 times in 500 attempts over 300 s**. A relaying radio
      now yields that slot, but **only while it is itself an active talker** — the
      unconditional form failed the mobility criterion, because on a sparse chain the single
      forward relay abstains and the frame dies there.

      | | PTT success | delivery | speech through |
      |---|---|---|---|
      | before | **9.8%** | 64.4% | 6.3% |
      | after | **100%** | 11.0% | 11.0% |

      **The lockout is fixed; concurrent calls still are not.** The second talker can now
      key up and nobody hears it, because one flood still saturates every slot in every
      neighbourhood — literature review §103, which [ADR-0011](decisions/0011-barrage-relaying.md)
      concedes it does not fix. That is **W-04**.
- [ ] **B-04c · `Simulation.delivery()` inflates a locked-out talker's score.** It divides by
      payloads that reached the air, so refused originations vanish from the denominator —
      a muted radio can read 64% delivered. `origination_attempts` now exists and PTT success
      is measurable, but the gate and scenarios do not report it. **Done when:** every
      delivery figure is quoted alongside PTT success, or multiplied into a speech-through
      number. *Hours.*
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
- [ ] **W-06b · Add `prevSender` to the header — 8 bits, and it buys a routing protocol.**
      [VINE](gotenna-vine.md) builds routing state purely by inspecting data-packet headers,
      and we already carry every field it needs except one. `prev` (added for
      [OQ-0018](open-questions.md#oq-0018)) is VINE's `sender`; `costFromSource` is derivable
      from `ttl`. **The whole additional cost is one 8-bit field**, taking FEC from 16% to
      14.6%. **Done when:** the header carries it and gradients are built from received
      frames in the C core. *Days. Prerequisite for W-04.*
- [ ] **W-04 · Controlled Barrage Regions.** The literature review's answer (§109) to
      concurrent calls: flood only within the corridor where d(s)+d(d) equals the path
      length. Fixes **B-07**, reduces airtime (the only remaining lever on
      [OQ-0026](open-questions.md#oq-0026)), and is what finally uses **B-09**'s `dst`.
      *Weeks. The single highest-value unbuilt item.*
      **Now has a published mechanism under it** — VINE's cost gradients *are* the hop counts
      CBR needs, obtained by header inspection rather than by flooding them. Do **W-06b**
      first. [gotenna-vine.md §2](gotenna-vine.md).
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
- [~] **H-04 · Chase NBWF's fate.** Partly answered: VINE cites *NATO STANAG 5631/AComP-5631,
      Narrowband Waveform Physical Layer, Ratification Draft, Edition 1, 2015*, so **NBWF
      reached a ratification-draft STANAG** and did not die with the 2011 report. Whether
      automatic relaying was ever added to it is still open. [gotenna-vine.md §6](gotenna-vine.md).
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
